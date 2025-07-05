#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <string>
#include <cassert>

class MutationOperators {
private:
    std::mt19937 rng;
    std::uniform_real_distribution<double> uniform_dist;
    std::normal_distribution<double> normal_dist;
    
public:
    MutationOperators() : rng(std::random_device{}()), 
                         uniform_dist(0.0, 1.0), 
                         normal_dist(0.0, 1.0) {}
    
    // ======================== BINARY REPRESENTATION ========================
    
    // Bit-flip mutation for binary strings
    void bitFlipMutation(std::vector<bool>& chromosome, double pm) {
        for (size_t i = 0; i < chromosome.size(); ++i) {
            if (uniform_dist(rng) < pm) {
                chromosome[i] = !chromosome[i];
            }
        }
    }
    
    // Alternative implementation using string
    void bitFlipMutation(std::string& binaryString, double pm) {
        for (size_t i = 0; i < binaryString.size(); ++i) {
            if (uniform_dist(rng) < pm) {
                binaryString[i] = (binaryString[i] == '0') ? '1' : '0';
            }
        }
    }
    
    // ======================== INTEGER REPRESENTATION ========================
    
    // Random resetting mutation for integer chromosomes
    void randomResettingMutation(std::vector<int>& chromosome, double pm, 
                                int minVal, int maxVal) {
        std::uniform_int_distribution<int> int_dist(minVal, maxVal);
        
        for (size_t i = 0; i < chromosome.size(); ++i) {
            if (uniform_dist(rng) < pm) {
                chromosome[i] = int_dist(rng);
            }
        }
    }
    
    // Creep mutation for integer chromosomes
    void creepMutation(std::vector<int>& chromosome, double pm, 
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
    
    // Uniform mutation for real-valued chromosomes
    void uniformMutation(std::vector<double>& chromosome, double pm,
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
    
    // Gaussian perturbation mutation
    void gaussianMutation(std::vector<double>& chromosome, double pm, 
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
    
    // Self-adaptive mutation with one step size
    struct SelfAdaptiveIndividual {
        std::vector<double> genes;
        double sigma;
        
        SelfAdaptiveIndividual(size_t size, double initial_sigma) 
            : genes(size), sigma(initial_sigma) {}
    };
    
    void selfAdaptiveMutation(SelfAdaptiveIndividual& individual,
                             const std::vector<double>& lowerBounds,
                             const std::vector<double>& upperBounds,
                             double tau = 0.1) {
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
    
    // Swap mutation for permutations
    void swapMutation(std::vector<int>& permutation, double pm) {
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
    
    // Insert mutation for permutations
    void insertMutation(std::vector<int>& permutation, double pm) {
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
    
    // Scramble mutation for permutations
    void scrambleMutation(std::vector<int>& permutation, double pm) {
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
    
    // Inversion mutation for permutations
    void inversionMutation(std::vector<int>& permutation, double pm) {
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
    
    // Variable-length list mutation (content and size)
    void listMutation(std::vector<int>& list, double pm_content, double pm_size,
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
    
    // Set seed for reproducible results
    void setSeed(unsigned int seed) {
        rng.seed(seed);
    }
    
    // Generate random binary chromosome
    std::vector<bool> generateBinaryChromosome(size_t length) {
        std::vector<bool> chromosome(length);
        for (size_t i = 0; i < length; ++i) {
            chromosome[i] = uniform_dist(rng) < 0.5;
        }
        return chromosome;
    }
    
    // Generate random integer chromosome
    std::vector<int> generateIntegerChromosome(size_t length, int minVal, int maxVal) {
        std::vector<int> chromosome(length);
        std::uniform_int_distribution<int> int_dist(minVal, maxVal);
        for (size_t i = 0; i < length; ++i) {
            chromosome[i] = int_dist(rng);
        }
        return chromosome;
    }
    
    // Generate random real-valued chromosome
    std::vector<double> generateRealChromosome(size_t length, 
                                              const std::vector<double>& lowerBounds,
                                              const std::vector<double>& upperBounds) {
        std::vector<double> chromosome(length);
        for (size_t i = 0; i < length; ++i) {
            std::uniform_real_distribution<double> range_dist(lowerBounds[i], upperBounds[i]);
            chromosome[i] = range_dist(rng);
        }
        return chromosome;
    }
    
    // Generate random permutation
    std::vector<int> generatePermutation(int n) {
        std::vector<int> permutation(n);
        std::iota(permutation.begin(), permutation.end(), 0);
        std::shuffle(permutation.begin(), permutation.end(), rng);
        return permutation;
    }
};

// ======================== DEMONSTRATION ========================

int main() {
    MutationOperators mutOp;
    mutOp.setSeed(42); // For reproducible results
    
    std::cout << "=== Genetic Algorithm Mutation Operators Demo ===\n\n";
    
    // Binary representation demo
    std::cout << "1. Binary Representation (Bit-flip Mutation):\n";
    auto binaryChrom = mutOp.generateBinaryChromosome(10);
    std::cout << "Original: ";
    for (bool bit : binaryChrom) std::cout << bit;
    std::cout << "\n";
    
    mutOp.bitFlipMutation(binaryChrom, 0.1);
    std::cout << "Mutated:  ";
    for (bool bit : binaryChrom) std::cout << bit;
    std::cout << "\n\n";
    
    // Integer representation demo
    std::cout << "2. Integer Representation (Random Resetting):\n";
    auto intChrom = mutOp.generateIntegerChromosome(8, 0, 9);
    std::cout << "Original: ";
    for (int val : intChrom) std::cout << val << " ";
    std::cout << "\n";
    
    mutOp.randomResettingMutation(intChrom, 0.2, 0, 9);
    std::cout << "Mutated:  ";
    for (int val : intChrom) std::cout << val << " ";
    std::cout << "\n\n";
    
    // Real-valued representation demo
    std::cout << "3. Real-valued Representation (Gaussian Mutation):\n";
    std::vector<double> lowerBounds = {-5.0, -5.0, -5.0, -5.0, -5.0};
    std::vector<double> upperBounds = {5.0, 5.0, 5.0, 5.0, 5.0};
    auto realChrom = mutOp.generateRealChromosome(5, lowerBounds, upperBounds);
    
    std::cout << "Original: ";
    for (double val : realChrom) std::cout << std::fixed << std::setprecision(3) << val << " ";
    std::cout << "\n";
    
    mutOp.gaussianMutation(realChrom, 0.3, 0.5, lowerBounds, upperBounds);
    std::cout << "Mutated:  ";
    for (double val : realChrom) std::cout << std::fixed << std::setprecision(3) << val << " ";
    std::cout << "\n\n";
    
    // Permutation representation demo
    std::cout << "4. Permutation Representation (Swap Mutation):\n";
    auto permChrom = mutOp.generatePermutation(8);
    std::cout << "Original: ";
    for (int val : permChrom) std::cout << val << " ";
    std::cout << "\n";
    
    mutOp.swapMutation(permChrom, 0.8);
    std::cout << "Mutated:  ";
    for (int val : permChrom) std::cout << val << " ";
    std::cout << "\n\n";
    
    // Self-adaptive mutation demo
    std::cout << "5. Self-adaptive Mutation:\n";
    MutationOperators::SelfAdaptiveIndividual individual(4, 1.0);
    individual.genes = {1.0, 2.0, 3.0, 4.0};
    
    std::cout << "Original genes: ";
    for (double gene : individual.genes) std::cout << std::fixed << std::setprecision(3) << gene << " ";
    std::cout << " (sigma: " << individual.sigma << ")\n";
    
    mutOp.selfAdaptiveMutation(individual, 
                              std::vector<double>(4, -10.0), 
                              std::vector<double>(4, 10.0));
    
    std::cout << "Mutated genes:  ";
    for (double gene : individual.genes) std::cout << std::fixed << std::setprecision(3) << gene << " ";
    std::cout << " (sigma: " << std::fixed << std::setprecision(3) << individual.sigma << ")\n";
    
    return 0;
}