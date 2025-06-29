#include "mutation.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <numeric>

MutationOperators::MutationOperators() : rng(std::random_device{}()), 
                     uniform_dist(0.0, 1.0), 
                     normal_dist(0.0, 1.0) {}

// ======================== BINARY REPRESENTATION ========================

void MutationOperators::bitFlipMutation(std::vector<bool>& chromosome, double pm) {
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            chromosome[i] = !chromosome[i];
        }
    }
}

void MutationOperators::bitFlipMutation(std::string& binaryString, double pm) {
    for (size_t i = 0; i < binaryString.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            binaryString[i] = (binaryString[i] == '0') ? '1' : '0';
        }
    }
}

// ======================== INTEGER REPRESENTATION ========================

void MutationOperators::randomResettingMutation(std::vector<int>& chromosome, double pm, 
                            int minVal, int maxVal) {
    std::uniform_int_distribution<int> int_dist(minVal, maxVal);
    
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            chromosome[i] = int_dist(rng);
        }
    }
}

void MutationOperators::creepMutation(std::vector<int>& chromosome, double pm, 
                  int stepSize, int minVal, int maxVal) {
    std::uniform_int_distribution<int> step_dist(-stepSize, stepSize);
    
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            int newVal = chromosome[i] + step_dist(rng);
            // Clamp to valid range
            chromosome[i] = std::max(minVal, std::min(maxVal, newVal));
        }
    }
}

// ======================== REAL-VALUED REPRESENTATION ========================

void MutationOperators::uniformMutation(std::vector<double>& chromosome, double pm,
                    const std::vector<double>& lowerBounds,
                    const std::vector<double>& upperBounds) {
    assert(chromosome.size() == lowerBounds.size());
    assert(chromosome.size() == upperBounds.size());
    
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (uniform_dist(rng) < pm) {
            std::uniform_real_distribution<double> range_dist(
                lowerBounds[i], upperBounds[i]);
            chromosome[i] = range_dist(rng);
        }
    }
}

void MutationOperators::gaussianMutation(std::vector<double>& chromosome, double pm, 
                     double sigma, const std::vector<double>& lowerBounds,
                     const std::vector<double>& upperBounds) {
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
    : genes(size), sigma(initial_sigma) {}

void MutationOperators::selfAdaptiveMutation(SelfAdaptiveIndividual& individual,
                         const std::vector<double>& lowerBounds,
                         const std::vector<double>& upperBounds,
                         double tau) {
    // Mutate step size first
    double gamma = normal_dist(rng);
    individual.sigma *= std::exp(tau * gamma);
    
    // Ensure sigma doesn't become too small
    individual.sigma = std::max(individual.sigma, 1e-6);
    
    // Mutate genes using the updated sigma
    std::normal_distribution<double> gene_dist(0.0, individual.sigma);
    
    for (size_t i = 0; i < individual.genes.size(); ++i) {
        individual.genes[i] += gene_dist(rng);
        
        // Clamp to valid range
        individual.genes[i] = std::max(lowerBounds[i], 
                                     std::min(upperBounds[i], 
                                            individual.genes[i]));
    }
}

// ======================== PERMUTATION REPRESENTATION ========================

void MutationOperators::swapMutation(std::vector<int>& permutation, double pm) {
    if (uniform_dist(rng) < pm && permutation.size() > 1) {
        std::uniform_int_distribution<size_t> pos_dist(0, permutation.size() - 1);
        size_t pos1 = pos_dist(rng);
        size_t pos2 = pos_dist(rng);
        
        // Ensure different positions
        while (pos1 == pos2) {
            pos2 = pos_dist(rng);
        }
        
        std::swap(permutation[pos1], permutation[pos2]);
    }
}

void MutationOperators::insertMutation(std::vector<int>& permutation, double pm) {
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

void MutationOperators::scrambleMutation(std::vector<int>& permutation, double pm) {
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

void MutationOperators::inversionMutation(std::vector<int>& permutation, double pm) {
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
                 int minVal, int maxVal, size_t minSize, size_t maxSize) {
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

std::vector<bool> MutationOperators::generateBinaryChromosome(size_t length) {
    std::vector<bool> chromosome(length);
    for (size_t i = 0; i < length; ++i) {
        chromosome[i] = uniform_dist(rng) < 0.5;
    }
    return chromosome;
}

std::vector<int> MutationOperators::generateIntegerChromosome(size_t length, int minVal, int maxVal) {
    std::vector<int> chromosome(length);
    std::uniform_int_distribution<int> int_dist(minVal, maxVal);
    for (size_t i = 0; i < length; ++i) {
        chromosome[i] = int_dist(rng);
    }
    return chromosome;
}

std::vector<double> MutationOperators::generateRealChromosome(size_t length, 
                                          const std::vector<double>& lowerBounds,
                                          const std::vector<double>& upperBounds) {
    std::vector<double> chromosome(length);
    for (size_t i = 0; i < length; ++i) {
        std::uniform_real_distribution<double> range_dist(lowerBounds[i], upperBounds[i]);
        chromosome[i] = range_dist(rng);
    }
    return chromosome;
}

std::vector<int> MutationOperators::generatePermutation(int n) {
    std::vector<int> permutation(n);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::shuffle(permutation.begin(), permutation.end(), rng);
    return permutation;
}