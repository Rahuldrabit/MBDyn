#include "hill_climbing.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <chrono>

// Constructor implementations
HillClimbing::HillClimbing() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()),
      evaluationCount_(0) {
    // Set default tweak and perturb functions
    tweakFunc_ = [this](const Solution& sol) { return defaultTweak(sol); };
    perturbFunc_ = [this](const Solution& sol, double strength) { 
        return defaultPerturb(sol, strength); 
    };
}

HillClimbing::HillClimbing(const FitnessFunction& fitnessFunc) 
    : HillClimbing() {
    fitnessFunc_ = fitnessFunc;
}

HillClimbing::HillClimbing(const FitnessFunction& fitnessFunc, const Config& config) 
    : HillClimbing(fitnessFunc) {
    config_ = config;
}

// Configuration methods
void HillClimbing::setConfig(const Config& config) {
    config_ = config;
}

void HillClimbing::setFitnessFunction(const FitnessFunction& fitnessFunc) {
    fitnessFunc_ = fitnessFunc;
}

void HillClimbing::setTweakFunction(const TweakFunction& tweakFunc) {
    tweakFunc_ = tweakFunc;
}

void HillClimbing::setPerturbFunction(const PerturbtFunction& perturbFunc) {
    perturbFunc_ = perturbFunc;
}

// Helper methods
HillClimbing::Solution HillClimbing::generateRandomSolution() const {
    Solution solution(config_.solutionSize);
    std::uniform_int_distribution<int> dist(0, 1);
    
    for (int& bit : solution) {
        bit = dist(rng_);
    }
    
    return solution;
}

HillClimbing::Solution HillClimbing::defaultTweak(const Solution& solution) const {
    Solution tweaked = solution;
    std::uniform_int_distribution<int> posDist(0, solution.size() - 1);
    std::uniform_real_distribution<double> probDist(0.0, 1.0);
    
    for (size_t i = 0; i < solution.size(); ++i) {
        if (probDist(rng_) < config_.mutationRate) {
            tweaked[i] = 1 - tweaked[i]; // Flip bit
        }
    }
    
    return tweaked;
}

HillClimbing::Solution HillClimbing::defaultPerturb(const Solution& solution, double strength) const {
    Solution perturbed = solution;
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    int numFlips = static_cast<int>(strength * solution.size());
    std::uniform_int_distribution<int> posDist(0, solution.size() - 1);
    
    for (int i = 0; i < numFlips; ++i) {
        int pos = posDist(rng_);
        perturbed[pos] = 1 - perturbed[pos];
    }
    
    return perturbed;
}

double HillClimbing::evaluateFitness(const Solution& solution) {
    if (!fitnessFunc_) {
        throw std::runtime_error("Fitness function not set");
    }
    
    evaluationCount_++;
    double fitness = fitnessFunc_(solution);
    fitnessHistory_.push_back(fitness);
    
    return fitness;
}

bool HillClimbing::isBetter(double fitness1, double fitness2) const {
    return fitness1 > fitness2; // Assuming maximization
}

void HillClimbing::reset() {
    evaluationCount_ = 0;
    fitnessHistory_.clear();
}

// Basic Hill Climbing Algorithm
HillClimbing::Result HillClimbing::basicHillClimbing() {
    return basicHillClimbing(generateRandomSolution());
}

HillClimbing::Result HillClimbing::basicHillClimbing(const Solution& initialSolution) {
    reset();
    Result result;
    
    Solution current = initialSolution;
    double currentFitness = evaluateFitness(current);
    
    result.bestSolution = current;
    result.bestFitness = currentFitness;
    
    while (evaluationCount_ < config_.maxEvaluations) {
        Solution neighbor = tweakFunc_(current);
        double neighborFitness = evaluateFitness(neighbor);
        
        if (isBetter(neighborFitness, currentFitness)) {
            current = neighbor;
            currentFitness = neighborFitness;
            
            if (isBetter(currentFitness, result.bestFitness)) {
                result.bestSolution = current;
                result.bestFitness = currentFitness;
            }
        }
        
        if (config_.verbose && evaluationCount_ % 1000 == 0) {
            std::cout << "Evaluations: " << evaluationCount_ 
                      << ", Best Fitness: " << result.bestFitness << std::endl;
        }
    }
    
    result.evaluationsUsed = evaluationCount_;
    result.fitnessHistory = fitnessHistory_;
    return result;
}

// Steepest Ascent Hill Climbing (SAHC)
HillClimbing::Result HillClimbing::steepestAscentHillClimbing() {
    return steepestAscentHillClimbing(generateRandomSolution());
}

HillClimbing::Result HillClimbing::steepestAscentHillClimbing(const Solution& initialSolution) {
    reset();
    Result result;
    
    Solution current = initialSolution;
    double currentFitness = evaluateFitness(current);
    
    result.bestSolution = current;
    result.bestFitness = currentFitness;
    
    while (evaluationCount_ < config_.maxEvaluations) {
        Solution bestNeighbor = current;
        double bestNeighborFitness = currentFitness;
        bool foundImprovement = false;
        
        // Evaluate all single-bit mutations
        for (size_t i = 0; i < current.size() && evaluationCount_ < config_.maxEvaluations; ++i) {
            Solution neighbor = current;
            neighbor[i] = 1 - neighbor[i]; // Flip bit i
            
            double neighborFitness = evaluateFitness(neighbor);
            
            if (isBetter(neighborFitness, bestNeighborFitness)) {
                bestNeighbor = neighbor;
                bestNeighborFitness = neighborFitness;
                foundImprovement = true;
            }
        }
        
        if (foundImprovement) {
            current = bestNeighbor;
            currentFitness = bestNeighborFitness;
            
            if (isBetter(currentFitness, result.bestFitness)) {
                result.bestSolution = current;
                result.bestFitness = currentFitness;
            }
        } else {
            // Local optimum reached, restart with random solution
            current = generateRandomSolution();
            currentFitness = evaluateFitness(current);
            result.restartsPerformed++;
        }
        
        if (config_.verbose && evaluationCount_ % 1000 == 0) {
            std::cout << "SAHC Evaluations: " << evaluationCount_ 
                      << ", Best Fitness: " << result.bestFitness << std::endl;
        }
    }
    
    result.evaluationsUsed = evaluationCount_;
    result.fitnessHistory = fitnessHistory_;
    return result;
}

// Next Ascent Hill Climbing (NAHC)
HillClimbing::Result HillClimbing::nextAscentHillClimbing() {
    return nextAscentHillClimbing(generateRandomSolution());
}

HillClimbing::Result HillClimbing::nextAscentHillClimbing(const Solution& initialSolution) {
    reset();
    Result result;
    
    Solution current = initialSolution;
    double currentFitness = evaluateFitness(current);
    
    result.bestSolution = current;
    result.bestFitness = currentFitness;
    
    size_t startPos = 0;
    
    while (evaluationCount_ < config_.maxEvaluations) {
        bool foundImprovement = false;
        
        // Check each bit position starting from startPos
        for (size_t i = 0; i < current.size() && evaluationCount_ < config_.maxEvaluations; ++i) {
            size_t pos = (startPos + i) % current.size();
            
            Solution neighbor = current;
            neighbor[pos] = 1 - neighbor[pos]; // Flip bit at pos
            
            double neighborFitness = evaluateFitness(neighbor);
            
            if (isBetter(neighborFitness, currentFitness)) {
                current = neighbor;
                currentFitness = neighborFitness;
                startPos = (pos + 1) % current.size(); // Continue from next position
                foundImprovement = true;
                
                if (isBetter(currentFitness, result.bestFitness)) {
                    result.bestSolution = current;
                    result.bestFitness = currentFitness;
                }
                break;
            }
        }
        
        if (!foundImprovement) {
            // Local optimum reached, restart with random solution
            current = generateRandomSolution();
            currentFitness = evaluateFitness(current);
            startPos = 0;
            result.restartsPerformed++;
        }
        
        if (config_.verbose && evaluationCount_ % 1000 == 0) {
            std::cout << "NAHC Evaluations: " << evaluationCount_ 
                      << ", Best Fitness: " << result.bestFitness << std::endl;
        }
    }
    
    result.evaluationsUsed = evaluationCount_;
    result.fitnessHistory = fitnessHistory_;
    return result;
}

// Random Mutation Hill Climbing (RMHC)
HillClimbing::Result HillClimbing::randomMutationHillClimbing() {
    return randomMutationHillClimbing(generateRandomSolution());
}

HillClimbing::Result HillClimbing::randomMutationHillClimbing(const Solution& initialSolution) {
    reset();
    Result result;
    
    Solution current = initialSolution;
    double currentFitness = evaluateFitness(current);
    
    result.bestSolution = current;
    result.bestFitness = currentFitness;
    
    std::uniform_int_distribution<int> posDist(0, current.size() - 1);
    
    while (evaluationCount_ < config_.maxEvaluations) {
        Solution neighbor = current;
        int pos = posDist(rng_);
        neighbor[pos] = 1 - neighbor[pos]; // Flip random bit
        
        double neighborFitness = evaluateFitness(neighbor);
        
        if (isBetter(neighborFitness, currentFitness) || neighborFitness == currentFitness) {
            current = neighbor;
            currentFitness = neighborFitness;
        }
        
        if (isBetter(currentFitness, result.bestFitness)) {
            result.bestSolution = current;
            result.bestFitness = currentFitness;
        }
        
        if (config_.verbose && evaluationCount_ % 1000 == 0) {
            std::cout << "RMHC Evaluations: " << evaluationCount_ 
                      << ", Best Fitness: " << result.bestFitness << std::endl;
        }
    }
    
    result.evaluationsUsed = evaluationCount_;
    result.fitnessHistory = fitnessHistory_;
    return result;
}

// Hill Climbing with Random Restarts
HillClimbing::Result HillClimbing::hillClimbingWithRestarts() {
    reset();
    Result result;
    
    std::uniform_int_distribution<int> intervalDist(config_.minRestartInterval, 
                                                   config_.maxRestartInterval);
    
    while (evaluationCount_ < config_.maxEvaluations) {
        Solution current = generateRandomSolution();
        double currentFitness = evaluateFitness(current);
        
        if (isBetter(currentFitness, result.bestFitness)) {
            result.bestSolution = current;
            result.bestFitness = currentFitness;
        }
        
        int restartInterval = intervalDist(rng_);
        int startEval = evaluationCount_;
        
        // Perform hill climbing for the restart interval
        while (evaluationCount_ < config_.maxEvaluations && 
               (evaluationCount_ - startEval) < restartInterval) {
            
            Solution neighbor = tweakFunc_(current);
            double neighborFitness = evaluateFitness(neighbor);
            
            if (isBetter(neighborFitness, currentFitness)) {
                current = neighbor;
                currentFitness = neighborFitness;
                
                if (isBetter(currentFitness, result.bestFitness)) {
                    result.bestSolution = current;
                    result.bestFitness = currentFitness;
                }
            }
        }
        
        result.restartsPerformed++;
        
        if (config_.verbose && result.restartsPerformed % 10 == 0) {
            std::cout << "Restarts: " << result.restartsPerformed 
                      << ", Evaluations: " << evaluationCount_
                      << ", Best Fitness: " << result.bestFitness << std::endl;
        }
    }
    
    result.evaluationsUsed = evaluationCount_;
    result.fitnessHistory = fitnessHistory_;
    return result;
}

// Iterated Local Search (ILS)
HillClimbing::Result HillClimbing::iteratedLocalSearch() {
    reset();
    Result result;
    
    // Initial solution and hill climbing
    Solution homeBase = generateRandomSolution();
    Result localResult = basicHillClimbing(homeBase);
    homeBase = localResult.bestSolution;
    
    result.bestSolution = homeBase;
    result.bestFitness = localResult.bestFitness;
    
    std::uniform_int_distribution<int> intervalDist(config_.minRestartInterval, 
                                                   config_.maxRestartInterval);
    
    while (evaluationCount_ < config_.maxEvaluations) {
        // Perturb the home base
        Solution perturbed = perturbFunc_(homeBase, config_.perturbationStrength);
        
        // Perform local hill climbing from perturbed solution
        int remainingEvals = config_.maxEvaluations - evaluationCount_;
        if (remainingEvals <= 0) break;
        
        // Save current state
        int savedEvals = evaluationCount_;
        Config tempConfig = config_;
        tempConfig.maxEvaluations = std::min(intervalDist(rng_), remainingEvals);
        
        // Perform local search
        Solution current = perturbed;
        double currentFitness = evaluateFitness(current);
        int localStartEval = evaluationCount_;
        
        while (evaluationCount_ < config_.maxEvaluations && 
               (evaluationCount_ - localStartEval) < tempConfig.maxEvaluations) {
            
            Solution neighbor = tweakFunc_(current);
            double neighborFitness = evaluateFitness(neighbor);
            
            if (isBetter(neighborFitness, currentFitness)) {
                current = neighbor;
                currentFitness = neighborFitness;
            }
        }
        
        // Update global best
        if (isBetter(currentFitness, result.bestFitness)) {
            result.bestSolution = current;
            result.bestFitness = currentFitness;
        }
        
        // Decide whether to update home base (simple strategy)
        if (isBetter(currentFitness, evaluateFitness(homeBase) - 1)) { // -1 for some tolerance
            homeBase = current;
        }
        
        result.restartsPerformed++;
        
        if (config_.verbose && result.restartsPerformed % 5 == 0) {
            std::cout << "ILS Iterations: " << result.restartsPerformed 
                      << ", Evaluations: " << evaluationCount_
                      << ", Best Fitness: " << result.bestFitness << std::endl;
        }
    }
    
    result.evaluationsUsed = evaluationCount_;
    result.fitnessHistory = fitnessHistory_;
    return result;
}

// Static utility functions for common problems
HillClimbing::FitnessFunction HillClimbing::oneMaxFitness() {
    return [](const Solution& solution) -> double {
        return static_cast<double>(std::count(solution.begin(), solution.end(), 1));
    };
}

HillClimbing::FitnessFunction HillClimbing::royalRoadFitness(int blockSize) {
    return [blockSize](const Solution& solution) -> double {
        double fitness = 0.0;
        int numBlocks = solution.size() / blockSize;
        
        for (int i = 0; i < numBlocks; ++i) {
            bool blockComplete = true;
            for (int j = 0; j < blockSize; ++j) {
                if (solution[i * blockSize + j] != 1) {
                    blockComplete = false;
                    break;
                }
            }
            if (blockComplete) {
                fitness += blockSize;
            }
        }
        
        return fitness;
    };
}

HillClimbing::FitnessFunction HillClimbing::rastriginFitness() {
    return [](const Solution& solution) -> double {
        // Convert binary to real values [-5.12, 5.12]
        std::vector<double> real = HillClimbingUtils::binaryToReal(solution, -5.12, 5.12);
        
        double sum = 0.0;
        for (double x : real) {
            sum += x * x - 10.0 * std::cos(2.0 * M_PI * x) + 10.0;
        }
        
        return -sum; // Negate for maximization
    };
}

HillClimbing::TweakFunction HillClimbing::bitFlipTweak(double mutationRate) {
    return [mutationRate](const Solution& solution) -> Solution {
        static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        
        Solution tweaked = solution;
        for (size_t i = 0; i < solution.size(); ++i) {
            if (dist(rng) < mutationRate) {
                tweaked[i] = 1 - tweaked[i];
            }
        }
        return tweaked;
    };
}

HillClimbing::TweakFunction HillClimbing::singleBitFlipTweak() {
    return [](const Solution& solution) -> Solution {
        static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<int> posDist(0, solution.size() - 1);
        
        Solution tweaked = solution;
        int pos = posDist(rng);
        tweaked[pos] = 1 - tweaked[pos];
        return tweaked;
    };
}

// Utility namespace implementations
namespace HillClimbingUtils {
    std::vector<double> binaryToReal(const HillClimbing::Solution& binary, 
                                   double minVal, double maxVal) {
        std::vector<double> real;
        
        // Assume each variable uses equal number of bits
        int bitsPerVar = 8; // Default
        int numVars = binary.size() / bitsPerVar;
        
        for (int i = 0; i < numVars; ++i) {
            unsigned int intVal = 0;
            for (int j = 0; j < bitsPerVar; ++j) {
                if (binary[i * bitsPerVar + j] == 1) {
                    intVal |= (1 << j);
                }
            }
            
            double normalizedVal = static_cast<double>(intVal) / ((1 << bitsPerVar) - 1);
            double realVal = minVal + normalizedVal * (maxVal - minVal);
            real.push_back(realVal);
        }
        
        return real;
    }
    
    HillClimbing::Solution realToBinary(const std::vector<double>& real, 
                                      double minVal, double maxVal, int bitsPerVar) {
        HillClimbing::Solution binary;
        
        for (double val : real) {
            double normalizedVal = (val - minVal) / (maxVal - minVal);
            unsigned int intVal = static_cast<unsigned int>(normalizedVal * ((1 << bitsPerVar) - 1));
            
            for (int j = 0; j < bitsPerVar; ++j) {
                binary.push_back((intVal >> j) & 1);
            }
        }
        
        return binary;
    }
    
    int hammingDistance(const HillClimbing::Solution& sol1, 
                       const HillClimbing::Solution& sol2) {
        int distance = 0;
        size_t minSize = std::min(sol1.size(), sol2.size());
        
        for (size_t i = 0; i < minSize; ++i) {
            if (sol1[i] != sol2[i]) {
                distance++;
            }
        }
        
        return distance;
    }
    
    void printSolution(const HillClimbing::Solution& solution) {
        for (int bit : solution) {
            std::cout << bit;
        }
        std::cout << std::endl;
    }
    
    void printResult(const HillClimbing::Result& result) {
        std::cout << "\n=== Hill Climbing Results ===" << std::endl;
        std::cout << "Best Fitness: " << std::fixed << std::setprecision(6) 
                  << result.bestFitness << std::endl;
        std::cout << "Evaluations Used: " << result.evaluationsUsed << std::endl;
        std::cout << "Restarts Performed: " << result.restartsPerformed << std::endl;
        std::cout << "Best Solution: ";
        printSolution(result.bestSolution);
        std::cout << "============================\n" << std::endl;
    }
}