#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <functional>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <cmath>
#include <memory>

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
    
    // Representation type
    enum RepresentationType { BINARY, REAL_VALUED, INTEGER, PERMUTATION } representation = REAL_VALUED;
    
    // Operator selections
    std::string crossoverType = "one_point";
    std::string mutationType = "gaussian";
    std::string selectionType = "tournament";
    
    // Output settings
    bool verbose = true;
    std::string outputFile = "ga_results.txt";
};

// Validation function for crossover-representation compatibility
bool validateCrossoverForRepresentation(GAConfig::RepresentationType rep, const std::string& crossover) {
    switch(rep) {
        case GAConfig::BINARY:
            return crossover == "one_point" || crossover == "two_point" || crossover == "uniform";
        case GAConfig::REAL_VALUED:
            return crossover == "arithmetic" || crossover == "blend" || crossover == "sbx" || 
                   crossover == "one_point" || crossover == "two_point" || crossover == "uniform";
        case GAConfig::INTEGER:
            return crossover == "one_point" || crossover == "two_point" || crossover == "uniform" || crossover == "arithmetic";
        case GAConfig::PERMUTATION:
            return crossover == "order_crossover" || crossover == "pmx" || crossover == "cycle";
        default:
            return false;
    }
}

// Validation function for mutation-representation compatibility
bool validateMutationForRepresentation(GAConfig::RepresentationType rep, const std::string& mutation) {
    switch(rep) {
        case GAConfig::BINARY:
            return mutation == "bit_flip";
        case GAConfig::REAL_VALUED:
            return mutation == "gaussian" || mutation == "uniform";
        case GAConfig::INTEGER:
            return mutation == "random_resetting" || mutation == "creep";
        case GAConfig::PERMUTATION:
            return mutation == "swap" || mutation == "insert" || mutation == "scramble" || mutation == "inversion";
        default:
            return false;
    }
}

// Get available crossover operators for a representation
std::vector<std::string> getAvailableCrossovers(GAConfig::RepresentationType rep) {
    switch(rep) {
        case GAConfig::BINARY:
            return {"one_point", "two_point", "uniform"};
        case GAConfig::REAL_VALUED:
            return {"arithmetic", "blend", "sbx", "one_point", "two_point", "uniform"};
        case GAConfig::INTEGER:
            return {"one_point", "two_point", "uniform", "arithmetic"};
        case GAConfig::PERMUTATION:
            return {"order_crossover", "pmx", "cycle"};
        default:
            return {};
    }
}

// Get available mutation operators for a representation
std::vector<std::string> getAvailableMutations(GAConfig::RepresentationType rep) {
    switch(rep) {
        case GAConfig::BINARY:
            return {"bit_flip"};
        case GAConfig::REAL_VALUED:
            return {"gaussian", "uniform"};
        case GAConfig::INTEGER:
            return {"random_resetting", "creep"};
        case GAConfig::PERMUTATION:
            return {"swap", "insert", "scramble", "inversion"};
        default:
            return {};
    }
}

// Factory functions for creating operators
std::unique_ptr<CrossoverOperator> createCrossoverOperator(const std::string& type) {
    if (type == "one_point") {
        return std::make_unique<OnePointCrossover>();
    } else if (type == "two_point") {
        return std::make_unique<TwoPointCrossover>();
    } else if (type == "uniform") {
        return std::make_unique<UniformCrossover>();
    } else if (type == "multi_point") {
        return std::make_unique<MultiPointCrossover>(3);
    } else if (type == "arithmetic") {
        return std::make_unique<IntermediateRecombination>();
    } else if (type == "blend") {
        return std::make_unique<BlendCrossover>(0.5);
    } else if (type == "sbx") {
        return std::make_unique<SimulatedBinaryCrossover>(2.0);
    } else if (type == "order_crossover") {
        return std::make_unique<OrderCrossover>();
    } else if (type == "pmx") {
        return std::make_unique<PartiallyMappedCrossover>();
    } else if (type == "cycle") {
        return std::make_unique<CycleCrossover>();
    }
    // Default to one point
    return std::make_unique<OnePointCrossover>();
}

std::unique_ptr<MutationOperators> createMutationOperator() {
    return std::make_unique<MutationOperators>();
}

// Base class for all individual representations
class BaseIndividual {
public:
    double fitness;
    
    BaseIndividual() : fitness(0.0) {}
    virtual ~BaseIndividual() = default;
    virtual Individual toIndividual() const = 0;
    virtual void randomInitialize(std::mt19937& rng, const GAConfig& config) = 0;
    virtual void clampToBounds(const GAConfig& config) = 0;
    virtual size_t size() const = 0;
};

// Real-valued individual
class RealValuedIndividual : public BaseIndividual {
public:
    std::vector<double> chromosome;
    
    RealValuedIndividual(int length) : chromosome(length) {}
    
    Individual toIndividual() const override {
        return Individual(chromosome, fitness);
    }
    
    void randomInitialize(std::mt19937& rng, const GAConfig& config) override {
        std::uniform_real_distribution<double> dist(config.lowerBound, config.upperBound);
        for (auto& gene : chromosome) {
            gene = dist(rng);
        }
    }
    
    void clampToBounds(const GAConfig& config) override {
        for (auto& gene : chromosome) {
            gene = std::max(config.lowerBound, std::min(config.upperBound, gene));
        }
    }
    
    size_t size() const override { return chromosome.size(); }
    
    static RealValuedIndividual fromIndividual(const Individual& ind) {
        RealValuedIndividual result(ind.genes.size());
        result.chromosome = ind.genes;
        result.fitness = ind.fitness;
        return result;
    }
};

// Binary individual
class BinaryIndividual : public BaseIndividual {
public:
    std::vector<bool> chromosome;
    
    BinaryIndividual(int length) : chromosome(length) {}
    
    Individual toIndividual() const override {
        std::vector<double> realChrom(chromosome.size());
        for (size_t i = 0; i < chromosome.size(); ++i) {
            realChrom[i] = chromosome[i] ? 1.0 : 0.0;
        }
        return Individual(realChrom, fitness);
    }
    
    void randomInitialize(std::mt19937& rng, const GAConfig& config) override {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        for (size_t i = 0; i < chromosome.size(); ++i) {
            chromosome[i] = dist(rng) < 0.5;
        }
    }
    
    void clampToBounds(const GAConfig& config) override {
        // Binary values don't need clamping
    }
    
    size_t size() const override { return chromosome.size(); }
};

// Integer individual
class IntegerIndividual : public BaseIndividual {
public:
    std::vector<int> chromosome;
    
    IntegerIndividual(int length) : chromosome(length) {}
    
    Individual toIndividual() const override {
        std::vector<double> realChrom(chromosome.size());
        for (size_t i = 0; i < chromosome.size(); ++i) {
            realChrom[i] = static_cast<double>(chromosome[i]);
        }
        return Individual(realChrom, fitness);
    }
    
    void randomInitialize(std::mt19937& rng, const GAConfig& config) override {
        std::uniform_int_distribution<int> dist(static_cast<int>(config.lowerBound), 
                                               static_cast<int>(config.upperBound));
        for (auto& gene : chromosome) {
            gene = dist(rng);
        }
    }
    
    void clampToBounds(const GAConfig& config) override {
        for (auto& gene : chromosome) {
            gene = std::max(static_cast<int>(config.lowerBound), 
                           std::min(static_cast<int>(config.upperBound), gene));
        }
    }
    
    size_t size() const override { return chromosome.size(); }
};

// Convenience typedef for compatibility
using GAIndividual = RealValuedIndividual;

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
    
    // Operators - now using dynamic selection
    std::unique_ptr<MutationOperators> mutationOp;
    std::unique_ptr<CrossoverOperator> crossoverOp;
    
public:
    SimpleGA(const GAConfig& cfg) : config(cfg), 
                                   rng(std::random_device{}()),
                                   realDist(cfg.lowerBound, cfg.upperBound),
                                   intDist(0, cfg.populationSize - 1),
                                   bestIndividual(cfg.chromosomeLength) {
        
        // Initialize operators based on config
        mutationOp = createMutationOperator();
        crossoverOp = createCrossoverOperator(config.crossoverType);
        
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
        
        if (config.verbose) {
            std::cout << "GA initialized with:" << std::endl;
            std::cout << "- Crossover: " << config.crossoverType << std::endl;
            std::cout << "- Mutation: " << config.mutationType << std::endl;
            std::cout << "- Selection: " << config.selectionType << std::endl;
        }
    }
    
    // Initialize population
    void initializePopulation() {
        population.clear();
        population.reserve(config.populationSize);
        
        for (int i = 0; i < config.populationSize; ++i) {
            GAIndividual individual(config.chromosomeLength);
            
            // Use the new randomInitialize method
            individual.randomInitialize(rng, config);
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
    
    // Crossover operation using dynamic operator
    std::pair<GAIndividual, GAIndividual> crossover(const GAIndividual& parent1, 
                                                   const GAIndividual& parent2) {
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        
        if (prob(rng) < config.crossoverRate) {
            GAIndividual child1(config.chromosomeLength);
            GAIndividual child2(config.chromosomeLength);
            
            if (config.representation == GAConfig::PERMUTATION) {
                // For permutation representation, use permutation-specific crossover
                std::vector<int> perm1(parent1.chromosome.size());
                std::vector<int> perm2(parent2.chromosome.size());
                
                // Convert to integer permutations
                for (size_t i = 0; i < parent1.chromosome.size(); ++i) {
                    perm1[i] = static_cast<int>(parent1.chromosome[i]);
                    perm2[i] = static_cast<int>(parent2.chromosome[i]);
                }
                
                // Use permutation crossover (cast to the expected interface)
                if (config.crossoverType == "order_crossover") {
                    auto* orderCrossover = dynamic_cast<OrderCrossover*>(crossoverOp.get());
                    if (orderCrossover) {
                        auto result = orderCrossover->crossover(perm1, perm2);
                        // Convert back to double
                        for (size_t i = 0; i < result.first.size(); ++i) {
                            child1.chromosome[i] = static_cast<double>(result.first[i]);
                            child2.chromosome[i] = static_cast<double>(result.second[i]);
                        }
                    }
                } else if (config.crossoverType == "pmx") {
                    auto* pmxCrossover = dynamic_cast<PartiallyMappedCrossover*>(crossoverOp.get());
                    if (pmxCrossover) {
                        auto result = pmxCrossover->crossover(perm1, perm2);
                        // Convert back to double
                        for (size_t i = 0; i < result.first.size(); ++i) {
                            child1.chromosome[i] = static_cast<double>(result.first[i]);
                            child2.chromosome[i] = static_cast<double>(result.second[i]);
                        }
                    }
                } else if (config.crossoverType == "cycle") {
                    auto* cycleCrossover = dynamic_cast<CycleCrossover*>(crossoverOp.get());
                    if (cycleCrossover) {
                        auto result = cycleCrossover->crossover(perm1, perm2);
                        // Convert back to double
                        for (size_t i = 0; i < result.first.size(); ++i) {
                            child1.chromosome[i] = static_cast<double>(result.first[i]);
                            child2.chromosome[i] = static_cast<double>(result.second[i]);
                        }
                    }
                }
            } else {
                // For other representations, use the standard crossover
                auto result = crossoverOp->crossover(parent1.chromosome, parent2.chromosome);
                child1.chromosome = result.first;
                child2.chromosome = result.second;
            }
            
            // Use the new clampToBounds method
            child1.clampToBounds(config);
            child2.clampToBounds(config);
            
            child1.fitness = evaluateFitness(child1.chromosome);
            child2.fitness = evaluateFitness(child2.chromosome);
            
            return {child1, child2};
        } else {
            return {parent1, parent2};
        }
    }
    
    // Mutation operation using dynamic operator
    void mutate(GAIndividual& individual) {
        // Use the selected mutation type based on representation
        if (config.representation == GAConfig::REAL_VALUED) {
            std::vector<double> bounds_lower(config.chromosomeLength, config.lowerBound);
            std::vector<double> bounds_upper(config.chromosomeLength, config.upperBound);
            
            if (config.mutationType == "gaussian") {
                mutationOp->gaussianMutation(individual.chromosome, config.mutationRate, 
                                           0.1, bounds_lower, bounds_upper);
            } else if (config.mutationType == "uniform") {
                mutationOp->uniformMutation(individual.chromosome, config.mutationRate, 
                                          bounds_lower, bounds_upper);
            } else {
                // Default to gaussian for real-valued
                mutationOp->gaussianMutation(individual.chromosome, config.mutationRate, 
                                           0.1, bounds_lower, bounds_upper);
            }
        } else if (config.representation == GAConfig::BINARY) {
            // For binary representation, we need to handle conversion
            std::vector<bool> binaryChrom(individual.chromosome.size());
            for (size_t i = 0; i < individual.chromosome.size(); ++i) {
                binaryChrom[i] = individual.chromosome[i] > 0.5;
            }
            
            if (config.mutationType == "bit_flip") {
                mutationOp->bitFlipMutation(binaryChrom, config.mutationRate);
            }
            
            // Convert back to double
            for (size_t i = 0; i < individual.chromosome.size(); ++i) {
                individual.chromosome[i] = binaryChrom[i] ? 1.0 : 0.0;
            }
        } else if (config.representation == GAConfig::INTEGER) {
            // For integer representation
            std::vector<int> intChrom(individual.chromosome.size());
            for (size_t i = 0; i < individual.chromosome.size(); ++i) {
                intChrom[i] = static_cast<int>(individual.chromosome[i]);
            }
            
            if (config.mutationType == "random_resetting") {
                mutationOp->randomResettingMutation(intChrom, config.mutationRate, 
                                                  static_cast<int>(config.lowerBound),
                                                  static_cast<int>(config.upperBound));
            } else if (config.mutationType == "creep") {
                mutationOp->creepMutation(intChrom, config.mutationRate, 1,
                                        static_cast<int>(config.lowerBound),
                                        static_cast<int>(config.upperBound));
            }
            
            // Convert back to double
            for (size_t i = 0; i < individual.chromosome.size(); ++i) {
                individual.chromosome[i] = static_cast<double>(intChrom[i]);
            }
        } else if (config.representation == GAConfig::PERMUTATION) {
            // For permutation representation
            std::vector<int> permChrom(individual.chromosome.size());
            for (size_t i = 0; i < individual.chromosome.size(); ++i) {
                permChrom[i] = static_cast<int>(individual.chromosome[i]);
            }
            
            if (config.mutationType == "swap") {
                mutationOp->swapMutation(permChrom, config.mutationRate);
            } else if (config.mutationType == "insert") {
                mutationOp->insertMutation(permChrom, config.mutationRate);
            } else if (config.mutationType == "scramble") {
                mutationOp->scrambleMutation(permChrom, config.mutationRate);
            } else if (config.mutationType == "inversion") {
                mutationOp->inversionMutation(permChrom, config.mutationRate);
            }
            
            // Convert back to double
            for (size_t i = 0; i < individual.chromosome.size(); ++i) {
                individual.chromosome[i] = static_cast<double>(permChrom[i]);
            }
        }
        
        individual.fitness = evaluateFitness(individual.chromosome);
    }
    
    // Tournament selection or roulette wheel selection
    GAIndividual selectParent() {
        if (config.selectionType == "tournament") {
            return selectParentUsingOperators();
        } else if (config.selectionType == "roulette") {
            return selectParentUsingOperators();
        } else {
            // Default to tournament
            return selectParentUsingOperators();
        }
    }
    
    // Use selection operators from the selection-operator module
    GAIndividual selectParentUsingOperators() {
        // Convert population to Individual format
        std::vector<Individual> individuals;
        for (const auto& ind : population) {
            individuals.push_back(ind.toIndividual());
        }
        
        std::vector<unsigned int> selectedIndices;
        
        if (config.selectionType == "tournament") {
            selectedIndices = TournamentSelection(individuals, 3); // Tournament size 3
        } else if (config.selectionType == "roulette") {
            selectedIndices = RouletteWheelSelection(individuals, 1);
        } else {
            // Default to tournament
            selectedIndices = TournamentSelection(individuals, 3);
        }
        
        // Return the selected individual
        if (!selectedIndices.empty()) {
            return population[selectedIndices[0]];
        } else {
            // Fallback to random selection
            std::uniform_int_distribution<int> dist(0, population.size() - 1);
            return population[dist(rng)];
        }
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
            
            // Elitism - keep best individuals using ElitismSelection operator
            int numElites = static_cast<int>(config.populationSize * config.eliteRatio);
            if (numElites > 0) {
                // Convert population to Individual format
                std::vector<Individual> individuals;
                for (const auto& ind : population) {
                    individuals.push_back(ind.toIndividual());
                }
                std::vector<unsigned int> eliteIndices = ElitismSelection(individuals, numElites);
                for (unsigned int idx : eliteIndices) {
                    newPopulation.push_back(GAIndividual::fromIndividual(individuals[idx]));
                }
            }
            
            // Generate offspring using selected operators
            while (newPopulation.size() < static_cast<size_t>(config.populationSize)) {
                GAIndividual parent1 = selectParent();
                GAIndividual parent2 = selectParent();
                
                auto children = crossover(parent1, parent2);
                
                mutate(children.first);
                mutate(children.second);
                
                newPopulation.push_back(children.first);
                if (newPopulation.size() < static_cast<size_t>(config.populationSize)) {
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
    void updateStatistics(int /* generation */) {
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
void runGATests(const std::string& crossoverType, const std::string& mutationType, 
               const std::string& selectionType, GAConfig::RepresentationType repType = GAConfig::REAL_VALUED) {
    std::cout << "=== GENETIC ALGORITHM TEST SUITE ===" << std::endl;
    std::cout << "Testing GA on benchmark optimization functions" << std::endl;
    std::cout << "Population: 50, Generations: 50, Variables: 10" << std::endl;
    
    std::string repTypeStr;
    switch(repType) {
        case GAConfig::BINARY: repTypeStr = "Binary"; break;
        case GAConfig::REAL_VALUED: repTypeStr = "Real-valued"; break;
        case GAConfig::INTEGER: repTypeStr = "Integer"; break;
        case GAConfig::PERMUTATION: repTypeStr = "Permutation"; break;
    }
    
    std::cout << "Representation: " << repTypeStr << ", Crossover: " << crossoverType 
              << ", Mutation: " << mutationType << ", Selection: " << selectionType << std::endl << std::endl;
    
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
        config.representation = repType;
        config.crossoverType = crossoverType;
        config.mutationType = mutationType;
        config.selectionType = selectionType;
        config.outputFile = "ga_" + funcName + "_" + repTypeStr + "_" + crossoverType + "_" + mutationType + "_" + selectionType + "_results.txt";
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
    std::string representationType, crossoverType, mutationType, selectionType;

    std::cout << "=== SIMPLE GENETIC ALGORITHM PIPELINE ===" << std::endl;
    
    // First ask for representation type
    std::cout << "Enter Representation type (binary, real_valued, integer, permutation): ";
    std::getline(std::cin, representationType);

    GAConfig::RepresentationType repType = GAConfig::REAL_VALUED; // default
    if (representationType == "binary") {
        repType = GAConfig::BINARY;
        std::cout << "Binary representation selected." << std::endl;
        std::cout << "Available crossovers: ";
        auto crossovers = getAvailableCrossovers(repType);
        for (size_t i = 0; i < crossovers.size(); ++i) {
            std::cout << crossovers[i];
            if (i < crossovers.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        std::cout << "Available mutations: ";
        auto mutations = getAvailableMutations(repType);
        for (size_t i = 0; i < mutations.size(); ++i) {
            std::cout << mutations[i];
            if (i < mutations.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    } else if (representationType == "real_valued") {
        repType = GAConfig::REAL_VALUED;
        std::cout << "Real-valued representation selected." << std::endl;
        std::cout << "Available crossovers: ";
        auto crossovers = getAvailableCrossovers(repType);
        for (size_t i = 0; i < crossovers.size(); ++i) {
            std::cout << crossovers[i];
            if (i < crossovers.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        std::cout << "Available mutations: ";
        auto mutations = getAvailableMutations(repType);
        for (size_t i = 0; i < mutations.size(); ++i) {
            std::cout << mutations[i];
            if (i < mutations.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    } else if (representationType == "integer") {
        repType = GAConfig::INTEGER;
        std::cout << "Integer representation selected." << std::endl;
        std::cout << "Available crossovers: ";
        auto crossovers = getAvailableCrossovers(repType);
        for (size_t i = 0; i < crossovers.size(); ++i) {
            std::cout << crossovers[i];
            if (i < crossovers.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        std::cout << "Available mutations: ";
        auto mutations = getAvailableMutations(repType);
        for (size_t i = 0; i < mutations.size(); ++i) {
            std::cout << mutations[i];
            if (i < mutations.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    } else if (representationType == "permutation") {
        repType = GAConfig::PERMUTATION;
        std::cout << "Permutation representation selected." << std::endl;
        std::cout << "Available crossovers: ";
        auto crossovers = getAvailableCrossovers(repType);
        for (size_t i = 0; i < crossovers.size(); ++i) {
            std::cout << crossovers[i];
            if (i < crossovers.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        std::cout << "Available mutations: ";
        auto mutations = getAvailableMutations(repType);
        for (size_t i = 0; i < mutations.size(); ++i) {
            std::cout << mutations[i];
            if (i < mutations.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    } else {
        std::cerr << "Invalid representation type. Defaulting to Real-valued." << std::endl;
        repType = GAConfig::REAL_VALUED;
    }
    
    // input crossover type
    std::cout << "Enter Crossover type: ";
    std::getline(std::cin, crossoverType);

    // Validate crossover type against representation
    if (!validateCrossoverForRepresentation(repType, crossoverType)) {
        std::cerr << "Invalid crossover '" << crossoverType << "' for " << representationType 
                  << " representation. Please choose from available options." << std::endl;
        return 1;
    }
    std::cout << "Using " << crossoverType << " Crossover" << std::endl;

    // input mutation type
    std::cout << "Enter Mutation type: ";
    std::getline(std::cin, mutationType);

    // Validate mutation type against representation
    if (!validateMutationForRepresentation(repType, mutationType)) {
        std::cerr << "Invalid mutation '" << mutationType << "' for " << representationType 
                  << " representation. Please choose from available options." << std::endl;
        return 1;
    }
    std::cout << "Using " << mutationType << " Mutation" << std::endl;
    
    // input selection type
    std::cout << "Enter Selection (tournament, roulette) type: ";
    std::getline(std::cin, selectionType);
    
    if (selectionType == "tournament" || selectionType == "roulette") {
        std::cout << "Using " << selectionType << " Selection" << std::endl;
    } else {
        std::cerr << "Invalid selection type. Defaulting to Tournament Selection." << std::endl;
        selectionType = "tournament";
    }

    try {
        runGATests(crossoverType, mutationType, selectionType, repType);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
