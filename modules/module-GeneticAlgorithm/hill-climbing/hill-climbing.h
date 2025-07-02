#ifndef HILL_CLIMBING_H
#define HILL_CLIMBING_H

#include <vector>
#include <functional>
#include <random>
#include <memory>

/**
 * Hill Climbing Optimization Library
 * 
 * This library implements various hill climbing algorithms including:
 * - Basic Hill Climbing
 * - Steepest Ascent Hill Climbing (SAHC)
 * - Next Ascent Hill Climbing (NAHC)
 * - Random Mutation Hill Climbing (RMHC)
 * - Hill Climbing with Random Restarts
 * - Iterated Local Search (ILS)
 */

class HillClimbing {
public:
    // Type definitions
    using Solution = std::vector<int>;
    using FitnessFunction = std::function<double(const Solution&)>;
    using TweakFunction = std::function<Solution(const Solution&)>;
    using PerturbtFunction = std::function<Solution(const Solution&, double)>;
    
    // Configuration structure
    struct Config {
        int maxEvaluations = 10000;
        int solutionSize = 32;
        double mutationRate = 0.1;
        int numRestarts = 10;
        int minRestartInterval = 100;
        int maxRestartInterval = 500;
        double perturbationStrength = 0.3;
        bool verbose = false;
        
        Config() = default;
    };
    
    // Result structure
    struct Result {
        Solution bestSolution;
        double bestFitness;
        int evaluationsUsed;
        int restartsPerformed;
        std::vector<double> fitnessHistory;
        
        Result() : bestFitness(-std::numeric_limits<double>::infinity()), 
                  evaluationsUsed(0), restartsPerformed(0) {}
    };

private:
    Config config_;
    FitnessFunction fitnessFunc_;
    TweakFunction tweakFunc_;
    PerturbtFunction perturbFunc_;
    std::mt19937 rng_;
    
    // Internal helper functions
    Solution generateRandomSolution() const;
    Solution defaultTweak(const Solution& solution) const;
    Solution defaultPerturb(const Solution& solution, double strength) const;
    double evaluateFitness(const Solution& solution);
    bool isBetter(double fitness1, double fitness2) const;
    
    // Statistics tracking
    mutable int evaluationCount_;
    mutable std::vector<double> fitnessHistory_;

public:
    // Constructors
    HillClimbing();
    HillClimbing(const FitnessFunction& fitnessFunc);
    HillClimbing(const FitnessFunction& fitnessFunc, const Config& config);
    
    // Configuration methods
    void setConfig(const Config& config);
    void setFitnessFunction(const FitnessFunction& fitnessFunc);
    void setTweakFunction(const TweakFunction& tweakFunc);
    void setPerturbFunction(const PerturbtFunction& perturbFunc);
    Config getConfig() const { return config_; }
    
    // Basic Hill Climbing Algorithm
    Result basicHillClimbing();
    Result basicHillClimbing(const Solution& initialSolution);
    
    // Steepest Ascent Hill Climbing (SAHC)
    Result steepestAscentHillClimbing();
    Result steepestAscentHillClimbing(const Solution& initialSolution);
    
    // Next Ascent Hill Climbing (NAHC)
    Result nextAscentHillClimbing();
    Result nextAscentHillClimbing(const Solution& initialSolution);
    
    // Random Mutation Hill Climbing (RMHC)
    Result randomMutationHillClimbing();
    Result randomMutationHillClimbing(const Solution& initialSolution);
    
    // Hill Climbing with Random Restarts
    Result hillClimbingWithRestarts();
    
    // Iterated Local Search (ILS)
    Result iteratedLocalSearch();
    
    // Utility methods
    void reset();
    int getEvaluationCount() const { return evaluationCount_; }
    std::vector<double> getFitnessHistory() const { return fitnessHistory_; }
    
    // Static utility functions for common problems
    static FitnessFunction oneMaxFitness();
    static FitnessFunction royalRoadFitness(int blockSize = 4);
    static FitnessFunction rastriginFitness();
    static TweakFunction bitFlipTweak(double mutationRate = 0.1);
    static TweakFunction singleBitFlipTweak();
};

// Inline helper functions
namespace HillClimbingUtils {
    // Convert binary solution to real values for continuous optimization
    std::vector<double> binaryToReal(const HillClimbing::Solution& binary, 
                                   double minVal, double maxVal);
    
    // Convert real values to binary solution
    HillClimbing::Solution realToBinary(const std::vector<double>& real, 
                                      double minVal, double maxVal, int bitsPerVar);
    
    // Hamming distance between two binary solutions
    int hammingDistance(const HillClimbing::Solution& sol1, 
                       const HillClimbing::Solution& sol2);
    
    // Print solution in a readable format
    void printSolution(const HillClimbing::Solution& solution);
    void printResult(const HillClimbing::Result& result);
}

#endif // HILL_CLIMBING_H