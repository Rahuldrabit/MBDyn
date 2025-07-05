#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <functional>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <cmath>

// Include our custom headers
#include "simple-GA-Test/fitness-function.h"
#include "selection-operator/selection-operator.h"
#include "crossover/crossover.h"
#include "mutation/mutation.h"

// Simple GA configuration structure
struct GAConfig {
    int populationSize = 50;
    int generations = 50;
    int chromosomeLength = 10;  // Number of variables
    double mutationRate = 0.01;
    double crossoverRate = 0.8;
    double eliteRatio = 0.1;
    
    // Function bounds
    double lowerBound = -5.12;
    double upperBound = 5.12;
    
    // Function selection
    enum FunctionType { RASTRIGIN, ACKLEY, SCHWEFEL } function = RASTRIGIN;
    
    // Output settings
    bool verbose = true;
    std::string outputFile = "ga_results.txt";
};

// Individual representation for real-valued GA
struct GAIndividual {
    std::vector<double> chromosome;
    double fitness;
    
    GAIndividual(int length) : chromosome(length), fitness(0.0) {}
    
    // Convert to Individual struct for selection operators
    Individual toIndividual() const {
        return Individual(chromosome, fitness);
    }
    
    // Create from Individual struct
    static GAIndividual fromIndividual(const Individual& ind) {
        GAIndividual result(ind.genes.size());
        result.chromosome = ind.genes;
        result.fitness = ind.fitness;
        return result;
    }
};

// Simple GA class implementation
class SimpleGA {
private:
    GAConfig config;
    std::vector<GAIndividual> population;
    std::mt19937 rng;
    std::uniform_real_distribution<double> realDist;
    std::uniform_int_distribution<int> intDist;
    
    // Statistics
    std::vector<double> bestFitnessHistory;
    std::vector<double> avgFitnessHistory;
    GAIndividual bestIndividual;
    
    // Operators
    std::unique_ptr<MutationOperators> mutationOp;
    std::unique_ptr<OnePointCrossover> crossoverOp;
    
public:
    SimpleGA(const GAConfig& cfg) : config(cfg), 
                                   rng(std::random_device{}()),
                                   realDist(cfg.lowerBound, cfg.upperBound),
                                   intDist(0, cfg.populationSize - 1),
                                   bestIndividual(cfg.chromosomeLength) {
        
        // Initialize operators
        mutationOp = std::make_unique<MutationOperators>();
        crossoverOp = std::make_unique<OnePointCrossover>();
        
        // Set function-specific bounds
        switch(config.function) {
            case GAConfig::RASTRIGIN:
                config.lowerBound = -5.12;
                config.upperBound = 5.12;
                break;
            case GAConfig::ACKLEY:
                config.lowerBound = -32.768;
                config.upperBound = 32.768;
                break;
            case GAConfig::SCHWEFEL:
                config.lowerBound = -500.0;
                config.upperBound = 500.0;
                break;
        }
        
        realDist = std::uniform_real_distribution<double>(config.lowerBound, config.upperBound);
    }
    
    // Initialize population
    void initializePopulation() {
        population.clear();
        population.reserve(config.populationSize);
        
        for (int i = 0; i < config.populationSize; ++i) {
            GAIndividual individual(config.chromosomeLength);
            
            // Random initialization
            for (int j = 0; j < config.chromosomeLength; ++j) {
                individual.chromosome[j] = realDist(rng);
            }
            
            individual.fitness = evaluateFitness(individual.chromosome);
            population.push_back(individual);
        }
        
        // Initialize best individual
        bestIndividual = *std::max_element(population.begin(), population.end(),
            [](const GAIndividual& a, const GAIndividual& b) {
                return a.fitness < b.fitness;
            });
    }
    
    // Evaluate fitness based on selected function
    double evaluateFitness(const std::vector<double>& chromosome) {
        switch(config.function) {
            case GAConfig::RASTRIGIN:
                return rastriginFitness(chromosome);
            case GAConfig::ACKLEY:
                return ackleyFitness(chromosome);
            case GAConfig::SCHWEFEL:
                return schwefelFitness(chromosome);
            default:
                return rastriginFitness(chromosome);
        }
    }
    
    // Tournament selection
    GAIndividual tournamentSelection() {
        const int tournamentSize = 3;
        GAIndividual best = population[intDist(rng)];
        
        for (int i = 1; i < tournamentSize; ++i) {
            GAIndividual candidate = population[intDist(rng)];
            if (candidate.fitness > best.fitness) {
                best = candidate;
            }
        }
        
        return best;
    }
    
    // Crossover operation
    std::pair<GAIndividual, GAIndividual> crossover(const GAIndividual& parent1, 
                                                   const GAIndividual& parent2) {
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        
        if (prob(rng) < config.crossoverRate) {
            auto result = crossoverOp->crossover(parent1.chromosome, parent2.chromosome);
            
            GAIndividual child1(config.chromosomeLength);
            GAIndividual child2(config.chromosomeLength);
            
            child1.chromosome = result.first;
            child2.chromosome = result.second;
            
            // Ensure bounds
            for (auto& gene : child1.chromosome) {
                gene = std::max(config.lowerBound, std::min(config.upperBound, gene));
            }
            for (auto& gene : child2.chromosome) {
                gene = std::max(config.lowerBound, std::min(config.upperBound, gene));
            }
            
            child1.fitness = evaluateFitness(child1.chromosome);
            child2.fitness = evaluateFitness(child2.chromosome);
            
            return {child1, child2};
        } else {
            return {parent1, parent2};
        }
    }
    
    // Mutation operation
    void mutate(GAIndividual& individual) {
        std::vector<double> bounds_lower(config.chromosomeLength, config.lowerBound);
        std::vector<double> bounds_upper(config.chromosomeLength, config.upperBound);
        
        mutationOp->gaussianMutation(individual.chromosome, config.mutationRate, 
                                   0.1, bounds_lower, bounds_upper);
        
        individual.fitness = evaluateFitness(individual.chromosome);
    }
    
    // Main GA evolution loop
    void evolve() {
        if (config.verbose) {
            std::cout << "Starting GA evolution for " << config.generations << " generations..." << std::endl;
            std::cout << "Function: ";
            switch(config.function) {
                case GAConfig::RASTRIGIN: std::cout << "Rastrigin"; break;
                case GAConfig::ACKLEY: std::cout << "Ackley"; break;
                case GAConfig::SCHWEFEL: std::cout << "Schwefel"; break;
            }
            std::cout << std::endl << std::endl;
        }
        
        for (int generation = 0; generation < config.generations; ++generation) {
            // Create new population
            std::vector<GAIndividual> newPopulation;
            newPopulation.reserve(config.populationSize);
            
            // Elitism - keep best individuals
            int numElites = static_cast<int>(config.populationSize * config.eliteRatio);
            std::vector<GAIndividual> sortedPop = population;
            std::sort(sortedPop.begin(), sortedPop.end(),
                [](const GAIndividual& a, const GAIndividual& b) {
                    return a.fitness > b.fitness;
                });
            
            for (int i = 0; i < numElites; ++i) {
                newPopulation.push_back(sortedPop[i]);
            }
            
            // Generate offspring
            while (newPopulation.size() < config.populationSize) {
                GAIndividual parent1 = tournamentSelection();
                GAIndividual parent2 = tournamentSelection();
                
                auto children = crossover(parent1, parent2);
                
                mutate(children.first);
                mutate(children.second);
                
                newPopulation.push_back(children.first);
                if (newPopulation.size() < config.populationSize) {
                    newPopulation.push_back(children.second);
                }
            }
            
            population = newPopulation;
            
            // Update statistics
            updateStatistics(generation);
            
            // Print progress
            if (config.verbose && generation % 10 == 0) {
                std::cout << "Generation " << std::setw(3) << generation 
                         << " | Best: " << std::setw(12) << std::fixed << std::setprecision(6) 
                         << bestFitnessHistory.back()
                         << " | Avg: " << std::setw(12) << std::fixed << std::setprecision(6)
                         << avgFitnessHistory.back() << std::endl;
            }
        }
        
        if (config.verbose) {
            std::cout << "\nEvolution completed!" << std::endl;
            printFinalResults();
        }
    }
    
    // Update statistics
    void updateStatistics(int generation) {
        double totalFitness = 0.0;
        double maxFitness = -std::numeric_limits<double>::infinity();
        GAIndividual* currentBest = nullptr;
        
        for (auto& individual : population) {
            totalFitness += individual.fitness;
            if (individual.fitness > maxFitness) {
                maxFitness = individual.fitness;
                currentBest = &individual;
            }
        }
        
        double avgFitness = totalFitness / config.populationSize;
        bestFitnessHistory.push_back(maxFitness);
        avgFitnessHistory.push_back(avgFitness);
        
        if (currentBest && currentBest->fitness > bestIndividual.fitness) {
            bestIndividual = *currentBest;
        }
    }
    
    // Print final results
    void printFinalResults() {
        std::cout << "\n=== FINAL RESULTS ===" << std::endl;
        std::cout << "Best fitness: " << bestIndividual.fitness << std::endl;
        std::cout << "Best chromosome: ";
        for (double gene : bestIndividual.chromosome) {
            std::cout << std::setw(10) << std::fixed << std::setprecision(4) << gene << " ";
        }
        std::cout << std::endl;
        
        // Calculate actual function value
        double actualValue = 0.0;
        switch(config.function) {
            case GAConfig::RASTRIGIN:
                actualValue = rastriginFunction(bestIndividual.chromosome);
                break;
            case GAConfig::ACKLEY:
                actualValue = ackleyFunction(bestIndividual.chromosome);
                break;
            case GAConfig::SCHWEFEL:
                actualValue = schwefelFunction(bestIndividual.chromosome);
                break;
        }
        std::cout << "Actual function value: " << actualValue << std::endl;
        
        // Save results to file
        saveResults();
    }
    
    // Save results to file
    void saveResults() {
        std::ofstream file(config.outputFile);
        if (file.is_open()) {
            file << "Generation,BestFitness,AvgFitness\n";
            for (size_t i = 0; i < bestFitnessHistory.size(); ++i) {
                file << i << "," << bestFitnessHistory[i] << "," << avgFitnessHistory[i] << "\n";
            }
            file.close();
            
            if (config.verbose) {
                std::cout << "Results saved to " << config.outputFile << std::endl;
            }
        }
    }
};

// Test function for running GA on all three functions
void runGATests() {
    std::cout << "=== GENETIC ALGORITHM TEST SUITE ===" << std::endl;
    std::cout << "Testing GA on benchmark optimization functions" << std::endl;
    std::cout << "Population: 50, Generations: 50, Variables: 10" << std::endl << std::endl;
    
    // Test configurations for each function
    std::vector<std::pair<GAConfig::FunctionType, std::string>> functions = {
        {GAConfig::RASTRIGIN, "rastrigin"},
        {GAConfig::ACKLEY, "ackley"},
        {GAConfig::SCHWEFEL, "schwefel"}
    };
    
    for (auto& [funcType, funcName] : functions) {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "Testing " << funcName << " function" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        GAConfig config;
        config.function = funcType;
        config.outputFile = "ga_" + funcName + "_results.txt";
        config.verbose = true;
        
        SimpleGA ga(config);
        ga.initializePopulation();
        
        auto start = std::chrono::high_resolution_clock::now();
        ga.evolve();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Execution time: " << duration.count() << " ms" << std::endl;
    }
    
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "All tests completed!" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

// Main function for testing
int main() {
    try {
        runGATests();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
