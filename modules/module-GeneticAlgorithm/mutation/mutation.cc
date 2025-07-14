#include "mutation.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <numeric>
#include <memory>

// ======================== MUTATION OPERATORS IMPLEMENTATION ========================

MutationOperators::MutationOperators() 
    : rng(std::random_device{}()), 
      uniform_dist(0.0, 1.0), 
      normal_dist(0.0, 1.0) {
}

void MutationOperators::validateProbability(double pm, const std::string& methodName) const {
    if (pm < 0.0 || pm > 1.0) {
        throw InvalidParameterException(methodName + " - probability must be in [0,1], got: " + std::to_string(pm));
    }
}

void MutationOperators::validateBounds(const std::vector<double>& lower, 
                                      const std::vector<double>& upper) const {
    if (lower.size() != upper.size()) {
        throw InvalidParameterException("Lower and upper bounds size mismatch: " + 
                         std::to_string(lower.size()) + " vs " + std::to_string(upper.size()));
    }
    
    for (size_t i = 0; i < lower.size(); ++i) {
        if (lower[i] > upper[i]) {
            throw InvalidParameterException("Invalid bounds at index " + std::to_string(i) + 
                             ": lower=" + std::to_string(lower[i]) + 
                             " > upper=" + std::to_string(upper[i]));
        }
    }
}

// ======================== BINARY REPRESENTATION ========================

void MutationOperators::bitFlipMutation(std::vector<bool>& chromosome, double pm) const {
    validateProbability(pm, "bitFlipMutation");
    
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            chromosome[i] = !chromosome[i];
        }
    }
}

void MutationOperators::bitFlipMutation(std::string& binaryString, double pm) const {
    validateProbability(pm, "bitFlipMutation(string)");
    
    for (size_t i = 0; i < binaryString.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            binaryString[i] = (binaryString[i] == '0') ? '1' : '0';
        }
    }
}

// ======================== INTEGER REPRESENTATION ========================

void MutationOperators::randomResettingMutation(std::vector<int>& chromosome, double pm, 
                                               int minVal, int maxVal) const {
    validateProbability(pm, "randomResettingMutation");
    
    if (minVal > maxVal) {
        throw InvalidParameterException("Invalid range: minVal=" + std::to_string(minVal) + 
                         " > maxVal=" + std::to_string(maxVal));
    }
    
    std::uniform_int_distribution<int> int_dist(minVal, maxVal);
    
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            chromosome[i] = int_dist(rng);
        }
    }
}

void MutationOperators::creepMutation(std::vector<int>& chromosome, double pm, 
                                     int stepSize, int minVal, int maxVal) const {
    validateProbability(pm, "creepMutation");
    
    if (minVal > maxVal || stepSize < 0) {
        throw InvalidParameterException("Invalid parameters: minVal=" + std::to_string(minVal) + 
                         ", maxVal=" + std::to_string(maxVal) + 
                         ", stepSize=" + std::to_string(stepSize));
    }
    
    std::uniform_int_distribution<int> step_dist(-stepSize, stepSize);
    
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            int newVal = chromosome[i] + step_dist(rng);
            chromosome[i] = std::max(minVal, std::min(maxVal, newVal));
        }
    }
}

// ======================== REAL-VALUED REPRESENTATION ========================

void MutationOperators::uniformMutation(std::vector<double>& chromosome, double pm,
                                       const std::vector<double>& lowerBounds,
                                       const std::vector<double>& upperBounds) const {
    validateProbability(pm, "uniformMutation");
    validateBounds(lowerBounds, upperBounds);
    
    if (chromosome.size() != lowerBounds.size()) {
        throw InvalidParameterException("Chromosome size mismatch with bounds: " + 
                         std::to_string(chromosome.size()) + " vs " + std::to_string(lowerBounds.size()));
    }
    
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            std::uniform_real_distribution<double> range_dist(lowerBounds[i], upperBounds[i]);
            chromosome[i] = range_dist(rng);
        }
    }
}

void MutationOperators::gaussianMutation(std::vector<double>& chromosome, double pm, 
                                        double sigma, const std::vector<double>& lowerBounds,
                                        const std::vector<double>& upperBounds) const {
    validateProbability(pm, "gaussianMutation");
    validateBounds(lowerBounds, upperBounds);
    
    if (sigma <= 0.0) {
        throw InvalidParameterException("Sigma must be positive, got: " + std::to_string(sigma));
    }
    
    if (chromosome.size() != lowerBounds.size()) {
        throw InvalidParameterException("Chromosome size mismatch with bounds");
    }
    
    std::normal_distribution<double> gauss_dist(0.0, sigma);
    
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            double perturbation = gauss_dist(rng);
            chromosome[i] += perturbation;
            
            // Clamp to valid range
            chromosome[i] = std::max(lowerBounds[i], 
                                   std::min(upperBounds[i], chromosome[i]));
        }
    }
}

MutationOperators::SelfAdaptiveIndividual::SelfAdaptiveIndividual(size_t size, double initial_sigma) 
    : genes(size), sigma(initial_sigma) {
    if (initial_sigma <= 0.0) {
        throw InvalidParameterException("Initial sigma must be positive");
    }
}

void MutationOperators::selfAdaptiveMutation(SelfAdaptiveIndividual& individual,
                                            const std::vector<double>& lowerBounds,
                                            const std::vector<double>& upperBounds,
                                            double tau) const {
    validateBounds(lowerBounds, upperBounds);
    
    if (tau <= 0.0) {
        throw InvalidParameterException("Tau must be positive, got: " + std::to_string(tau));
    }
    
    // Mutate step size first
    double gamma = normal_dist(rng);
    individual.sigma *= std::exp(tau * gamma);
    
    // Ensure sigma doesn't become too small or too large
    individual.sigma = std::max(individual.sigma, 1e-6);
    individual.sigma = std::min(individual.sigma, 100.0);
    
    // Mutate genes using the updated sigma
    std::normal_distribution<double> gene_dist(0.0, individual.sigma);
    
    for (size_t i = 0; i < individual.genes.size(); ++i) {
        individual.genes[i] += gene_dist(rng);
        
        // Clamp to valid range
        individual.genes[i] = std::max(lowerBounds[i], 
                                     std::min(upperBounds[i], individual.genes[i]));
    }
}

// ======================== PERMUTATION REPRESENTATION ========================

void MutationOperators::swapMutation(std::vector<int>& permutation, double pm) const {
    validateProbability(pm, "swapMutation");
    
    if (permutation.empty()) {
        return;
    }
    
    if (uniform_dist(rng) < pm && permutation.size() > 1) {
        std::uniform_int_distribution<size_t> pos_dist(0, permutation.size() - 1);
        size_t pos1 = pos_dist(rng);
        size_t pos2 = pos_dist(rng);
        
        // Ensure different positions
        size_t attempts = 0;
        while (pos1 == pos2 && attempts < 100) {
            pos2 = pos_dist(rng);
            ++attempts;
        }
        
        if (pos1 != pos2) {
            std::swap(permutation[pos1], permutation[pos2]);
        }
    }
}

void MutationOperators::insertMutation(std::vector<int>& permutation, double pm) const {
    validateProbability(pm, "insertMutation");
    
    if (uniform_dist(rng) < pm && permutation.size() > 1) {
        std::uniform_int_distribution<size_t> pos_dist(0, permutation.size() - 1);
        size_t pos1 = pos_dist(rng);
        size_t pos2 = pos_dist(rng);
        
        if (pos1 != pos2) {
            int value = permutation[pos1];
            permutation.erase(permutation.begin() + pos1);
            
            // Adjust pos2 if necessary
            if (pos2 > pos1) pos2--;
            
            permutation.insert(permutation.begin() + pos2, value);
        }
    }
}

void MutationOperators::scrambleMutation(std::vector<int>& permutation, double pm) const {
    validateProbability(pm, "scrambleMutation");
    
    if (uniform_dist(rng) < pm && permutation.size() > 1) {
        std::uniform_int_distribution<size_t> pos_dist(0, permutation.size() - 1);
        size_t start = pos_dist(rng);
        size_t end = pos_dist(rng);
        
        if (start > end) std::swap(start, end);
        
        // Scramble the subsequence
        std::shuffle(permutation.begin() + start, 
                    permutation.begin() + end + 1, rng);
    }
}

void MutationOperators::inversionMutation(std::vector<int>& permutation, double pm) const {
    validateProbability(pm, "inversionMutation");
    
    if (uniform_dist(rng) < pm && permutation.size() > 1) {
        std::uniform_int_distribution<size_t> pos_dist(0, permutation.size() - 1);
        size_t start = pos_dist(rng);
        size_t end = pos_dist(rng);
        
        if (start > end) std::swap(start, end);
        
        // Reverse the subsequence
        std::reverse(permutation.begin() + start, 
                    permutation.begin() + end + 1);
    }
}

// ======================== LIST REPRESENTATION ========================

void MutationOperators::listMutation(std::vector<int>& list, double pm_content, double pm_size,
                                     int minVal, int maxVal, size_t minSize, size_t maxSize) const {
    validateProbability(pm_content, "listMutation(content)");
    validateProbability(pm_size, "listMutation(size)");
    
    if (minVal > maxVal || minSize > maxSize) {
        throw InvalidParameterException("Invalid parameters for listMutation");
    }
    
    // Mutate content first
    std::uniform_int_distribution<int> val_dist(minVal, maxVal);
    for (size_t i = 0; i < list.size(); ++i) {
        if (uniform_dist(rng) < pm_content) {
            list[i] = val_dist(rng);
        }
    }
    
    // Mutate size
    if (uniform_dist(rng) < pm_size) {
        std::uniform_int_distribution<int> size_change(-1, 1);
        int change = size_change(rng);
        
        size_t newSize = list.size() + change;
        newSize = std::max(minSize, std::min(maxSize, newSize));
        
        if (newSize > list.size()) {
            // Add random elements
            for (size_t i = list.size(); i < newSize; ++i) {
                list.push_back(val_dist(rng));
            }
        } else if (newSize < list.size()) {
            // Remove elements
            list.resize(newSize);
        }
    }
}

// ======================== UTILITY FUNCTIONS ========================

void MutationOperators::setSeed(unsigned int seed) {
    rng.seed(seed);
}

std::vector<bool> MutationOperators::generateBinaryChromosome(size_t length) const {
    std::vector<bool> chromosome(length);
    for (size_t i = 0; i < length; ++i) {
        chromosome[i] = uniform_dist(rng) < 0.5;
    }
    return chromosome;
}

std::vector<int> MutationOperators::generateIntegerChromosome(size_t length, int minVal, int maxVal) const {
    std::vector<int> chromosome(length);
    std::uniform_int_distribution<int> int_dist(minVal, maxVal);
    for (size_t i = 0; i < length; ++i) {
        chromosome[i] = int_dist(rng);
    }
    return chromosome;
}

std::vector<double> MutationOperators::generateRealChromosome(size_t length, 
                                                             const std::vector<double>& lowerBounds,
                                                             const std::vector<double>& upperBounds) const {
    validateBounds(lowerBounds, upperBounds);
    
    if (length != lowerBounds.size()) {
        throw InvalidParameterException("Length mismatch with bounds");
    }
    
    std::vector<double> chromosome(length);
    for (size_t i = 0; i < length; ++i) {
        std::uniform_real_distribution<double> range_dist(lowerBounds[i], upperBounds[i]);
        chromosome[i] = range_dist(rng);
    }
    return chromosome;
}

std::vector<int> MutationOperators::generatePermutation(int n) const {
    if (n < 0) {
        throw InvalidParameterException("Permutation size must be non-negative");
    }
    
    std::vector<int> permutation(n);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::shuffle(permutation.begin(), permutation.end(), rng);
    return permutation;
}