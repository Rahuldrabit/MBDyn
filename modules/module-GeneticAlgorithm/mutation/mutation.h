#ifndef MUTATION_H
#define MUTATION_H

#include <vector>
#include <random>
#include <string>
#include <memory>
#include <stdexcept>
#include <sstream>

// Forward declaration for logger
class Logger;

/**
 * @brief Comprehensive mutation operators for genetic algorithms
 * 
 * This class provides various mutation operators for different chromosome
 * representations including binary, integer, real-valued, permutation,
 * and variable-length list representations.
 */
class MutationOperators {
public:
    // Exception classes for better error handling
    class MutationException : public std::runtime_error {
    public:
        explicit MutationException(const std::string& msg) : std::runtime_error(msg) {}
    };

    class InvalidParameterException : public MutationException {
    public:
        explicit InvalidParameterException(const std::string& msg) 
            : MutationException("Invalid parameter: " + msg) {}
    };

    // Statistics tracking structure
    struct MutationStats {
        size_t totalMutations = 0;
        size_t successfulMutations = 0;
        size_t failedMutations = 0;
        double averagePerturbation = 0.0;
        
        void reset() {
            totalMutations = successfulMutations = failedMutations = 0;
            averagePerturbation = 0.0;
        }
    };

private:
    mutable std::mt19937 rng;
    mutable std::uniform_real_distribution<double> uniform_dist;
    mutable std::normal_distribution<double> normal_dist;
    std::shared_ptr<Logger> logger;
    mutable MutationStats stats;
    
    // Helper methods
    void validateProbability(double pm, const std::string& methodName) const;
    void validateBounds(const std::vector<double>& lower, const std::vector<double>& upper) const;
    void logMutationAttempt(const std::string& methodName, double pm, size_t chromosomeSize) const;
    void logMutationResult(const std::string& methodName, bool success, const std::string& details = "") const;
    
public:
    explicit MutationOperators(std::shared_ptr<Logger> logger = nullptr);
    
    // ======================== BINARY REPRESENTATION ========================
    
    /**
     * @brief Bit-flip mutation for binary chromosomes
     * @param chromosome Binary chromosome to mutate
     * @param pm Mutation probability per bit
     * @throws InvalidParameterException if pm is not in [0,1]
     */
    void bitFlipMutation(std::vector<bool>& chromosome, double pm) const;
    
    /**
     * @brief Bit-flip mutation for binary string representation
     * @param binaryString Binary string to mutate
     * @param pm Mutation probability per bit
     * @throws InvalidParameterException if pm is not in [0,1]
     */
    void bitFlipMutation(std::string& binaryString, double pm) const;
    
    // ======================== INTEGER REPRESENTATION ========================
    
    /**
     * @brief Random resetting mutation for integer chromosomes
     * @param chromosome Integer chromosome to mutate
     * @param pm Mutation probability per gene
     * @param minVal Minimum value for genes
     * @param maxVal Maximum value for genes
     * @throws InvalidParameterException if parameters are invalid
     */
    void randomResettingMutation(std::vector<int>& chromosome, double pm, 
                                int minVal, int maxVal) const;
    
    /**
     * @brief Creep mutation for integer chromosomes
     * @param chromosome Integer chromosome to mutate
     * @param pm Mutation probability per gene
     * @param stepSize Maximum step size for mutation
     * @param minVal Minimum value for genes
     * @param maxVal Maximum value for genes
     * @throws InvalidParameterException if parameters are invalid
     */
    void creepMutation(std::vector<int>& chromosome, double pm, 
                      int stepSize, int minVal, int maxVal) const;
    
    // ======================== REAL-VALUED REPRESENTATION ========================
    
    /**
     * @brief Uniform mutation for real-valued chromosomes
     * @param chromosome Real-valued chromosome to mutate
     * @param pm Mutation probability per gene
     * @param lowerBounds Lower bounds for each gene
     * @param upperBounds Upper bounds for each gene
     * @throws InvalidParameterException if bounds are inconsistent
     */
    void uniformMutation(std::vector<double>& chromosome, double pm,
                        const std::vector<double>& lowerBounds,
                        const std::vector<double>& upperBounds) const;
    
    /**
     * @brief Gaussian perturbation mutation
     * @param chromosome Real-valued chromosome to mutate
     * @param pm Mutation probability per gene
     * @param sigma Standard deviation for Gaussian perturbation
     * @param lowerBounds Lower bounds for each gene
     * @param upperBounds Upper bounds for each gene
     * @throws InvalidParameterException if parameters are invalid
     */
    void gaussianMutation(std::vector<double>& chromosome, double pm, 
                         double sigma, const std::vector<double>& lowerBounds,
                         const std::vector<double>& upperBounds) const;
    
    // Self-adaptive mutation with one step size
    struct SelfAdaptiveIndividual {
        std::vector<double> genes;
        double sigma;
        
        SelfAdaptiveIndividual(size_t size, double initial_sigma);
    };
    
    /**
     * @brief Self-adaptive mutation with evolving step size
     */
    void selfAdaptiveMutation(SelfAdaptiveIndividual& individual,
                             const std::vector<double>& lowerBounds,
                             const std::vector<double>& upperBounds,
                             double tau = 0.1) const;
    
    // ======================== PERMUTATION REPRESENTATION ========================
    
    /**
     * @brief Swap mutation for permutations
     */
    void swapMutation(std::vector<int>& permutation, double pm) const;
    
    /**
     * @brief Insert mutation for permutations
     */
    void insertMutation(std::vector<int>& permutation, double pm) const;
    
    /**
     * @brief Scramble mutation for permutations
     */
    void scrambleMutation(std::vector<int>& permutation, double pm) const;
    
    /**
     * @brief Inversion mutation for permutations
     */
    void inversionMutation(std::vector<int>& permutation, double pm) const;
    
    // ======================== LIST REPRESENTATION ========================
    
    /**
     * @brief Variable-length list mutation (content and size)
     */
    void listMutation(std::vector<int>& list, double pm_content, double pm_size,
                     int minVal, int maxVal, size_t minSize, size_t maxSize) const;
    
    // ======================== UTILITY FUNCTIONS ========================
    
    void setSeed(unsigned int seed);
    void setLogger(std::shared_ptr<Logger> newLogger);
    const MutationStats& getStatistics() const { return stats; }
    void resetStatistics() { stats.reset(); }
    
    // Generator functions
    std::vector<bool> generateBinaryChromosome(size_t length) const;
    std::vector<int> generateIntegerChromosome(size_t length, int minVal, int maxVal) const;
    std::vector<double> generateRealChromosome(size_t length, 
                                              const std::vector<double>& lowerBounds,
                                              const std::vector<double>& upperBounds) const;
    std::vector<int> generatePermutation(int n) const;
};

/**
 * @brief Simple logger interface for mutation operations
 */
class Logger {
public:
    enum Level { DEBUG, INFO, WARNING, ERROR };
    
    virtual ~Logger() = default;
    virtual void log(Level level, const std::string& message) = 0;
    
    void debug(const std::string& msg) { log(DEBUG, msg); }
    void info(const std::string& msg) { log(INFO, msg); }
    void warning(const std::string& msg) { log(WARNING, msg); }
    void error(const std::string& msg) { log(ERROR, msg); }
};

/**
 * @brief Console logger implementation
 */
class ConsoleLogger : public Logger {
private:
    Level minLevel;
    
public:
    explicit ConsoleLogger(Level minLevel = INFO) : minLevel(minLevel) {}
    void log(Level level, const std::string& message) override;
};

#endif