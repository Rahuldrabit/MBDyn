#ifndef MUTATION_H
#define MUTATION_H

#include <vector>
#include <random>
#include <string>

class MutationOperators {
private:
    std::mt19937 rng;
    std::uniform_real_distribution<double> uniform_dist;
    std::normal_distribution<double> normal_dist;
    
public:
    MutationOperators();
    
    // ======================== BINARY REPRESENTATION ========================
    
    // Bit-flip mutation for binary strings
    void bitFlipMutation(std::vector<bool>& chromosome, double pm);
    
    // Alternative implementation using string
    void bitFlipMutation(std::string& binaryString, double pm);
    
    // ======================== INTEGER REPRESENTATION ========================
    
    // Random resetting mutation for integer chromosomes
    void randomResettingMutation(std::vector<int>& chromosome, double pm, 
                                int minVal, int maxVal);
    
    // Creep mutation for integer chromosomes
    void creepMutation(std::vector<int>& chromosome, double pm, 
                      int stepSize, int minVal, int maxVal);
    
    // ======================== REAL-VALUED REPRESENTATION ========================
    
    // Uniform mutation for real-valued chromosomes
    void uniformMutation(std::vector<double>& chromosome, double pm,
                        const std::vector<double>& lowerBounds,
                        const std::vector<double>& upperBounds);
    
    // Gaussian perturbation mutation
    void gaussianMutation(std::vector<double>& chromosome, double pm, 
                         double sigma, const std::vector<double>& lowerBounds,
                         const std::vector<double>& upperBounds);
    
    // Self-adaptive mutation with one step size
    struct SelfAdaptiveIndividual {
        std::vector<double> genes;
        double sigma;
        
        SelfAdaptiveIndividual(size_t size, double initial_sigma);
    };
    
    void selfAdaptiveMutation(SelfAdaptiveIndividual& individual,
                             const std::vector<double>& lowerBounds,
                             const std::vector<double>& upperBounds,
                             double tau = 0.1);
    
    // ======================== PERMUTATION REPRESENTATION ========================
    
    // Swap mutation for permutations
    void swapMutation(std::vector<int>& permutation, double pm);
    
    // Insert mutation for permutations
    void insertMutation(std::vector<int>& permutation, double pm);
    
    // Scramble mutation for permutations
    void scrambleMutation(std::vector<int>& permutation, double pm);
    
    // Inversion mutation for permutations
    void inversionMutation(std::vector<int>& permutation, double pm);
    
    // ======================== LIST REPRESENTATION ========================
    
    // Variable-length list mutation (content and size)
    void listMutation(std::vector<int>& list, double pm_content, double pm_size,
                     int minVal, int maxVal, size_t minSize, size_t maxSize);
    
    // ======================== UTILITY FUNCTIONS ========================
    
    // Set seed for reproducible results
    void setSeed(unsigned int seed);
    
    // Generate random binary chromosome
    std::vector<bool> generateBinaryChromosome(size_t length);
    
    // Generate random integer chromosome
    std::vector<int> generateIntegerChromosome(size_t length, int minVal, int maxVal);
    
    // Generate random real-valued chromosome
    std::vector<double> generateRealChromosome(size_t length, 
                                              const std::vector<double>& lowerBounds,
                                              const std::vector<double>& upperBounds);
    
    // Generate random permutation
    std::vector<int> generatePermutation(int n);
};

#endif