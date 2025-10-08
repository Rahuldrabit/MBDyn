/*
 * Unified GA Library Implementation for MBDyn Module
 * Automatically selects operators by name from your existing implementations
 * Single file that replaces libga_real.cc, libga_complete.cc, libga_stub.cc
 */

#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cassert>
#include <numeric>
#include <iomanip>
#include <map>
#include <functional>

// Include your existing operator implementations
#include "crossover/crossover.h"
#include "mutation/mutation.h" 
#include "selection-operator/selection-operator.h"

// Use the Individual from selection-operator header to avoid conflicts
using GAIndividual = Individual;

// Forward declare fitness function if available
#ifdef INCLUDE_FITNESS_FUNCTION
#include "simple-GA-Test/fitness-function.h"
#else
// Simple fitness function fallback
class SimpleFitnessFunction {
public:
    virtual double evaluate(const std::vector<double>& genes) {
        double fitness = 0.0;
        for (double x : genes) {
            fitness += x * x - 10.0 * cos(2.0 * M_PI * x) + 10.0;
        }
        return -fitness; // Negative because we want to maximize (minimize Rastrigin)
    }
};
#endif

// Base interfaces for operator factories
class ICrossoverOperator {
public:
    virtual ~ICrossoverOperator() = default;
    virtual std::pair<std::vector<double>, std::vector<double>> crossover(
        const std::vector<double>& parent1, const std::vector<double>& parent2) = 0;
    virtual std::string getName() const = 0; 
};

class IMutationOperator {
public:
    virtual ~IMutationOperator() = default;
    virtual void mutate(std::vector<double>& chromosome, double pm,
                       const std::vector<double>& lowerBounds,
                       const std::vector<double>& upperBounds) = 0;
    virtual std::string getName() const = 0;
};

class ISelectionOperator {
public:
    virtual ~ISelectionOperator() = default;
    virtual Individual select(const std::vector<Individual>& population, std::mt19937& rng) = 0;
    virtual std::string getName() const = 0;
};

// Wrapper classes for your existing operators
class TwoPointCrossoverWrapper : public ICrossoverOperator {
private:
    TwoPointCrossover impl;
public:
    std::pair<std::vector<double>, std::vector<double>> crossover(
        const std::vector<double>& parent1, const std::vector<double>& parent2) override {
        return impl.crossover(parent1, parent2);
    }
    std::string getName() const override { return "two_point"; }
};

class OnePointCrossoverWrapper : public ICrossoverOperator {
private:
    OnePointCrossover impl;
public:
    std::pair<std::vector<double>, std::vector<double>> crossover(
        const std::vector<double>& parent1, const std::vector<double>& parent2) override {
        return impl.crossover(parent1, parent2);
    }
    std::string getName() const override { return "one_point"; }
};

class UniformCrossoverWrapper : public ICrossoverOperator {
private:
    UniformCrossover impl;
public:
    std::pair<std::vector<double>, std::vector<double>> crossover(
        const std::vector<double>& parent1, const std::vector<double>& parent2) override {
        return impl.crossover(parent1, parent2);
    }
    std::string getName() const override { return "uniform"; }
};

class BlendCrossoverWrapper : public ICrossoverOperator {
private:
    BlendCrossover impl;
public:
    std::pair<std::vector<double>, std::vector<double>> crossover(
        const std::vector<double>& parent1, const std::vector<double>& parent2) override {
        return impl.crossover(parent1, parent2);
    }
    std::string getName() const override { return "blend"; }
};

class UniformMutationWrapper : public IMutationOperator {
private:
    MutationOperators impl;
public:
    void mutate(std::vector<double>& chromosome, double pm,
               const std::vector<double>& lowerBounds,
               const std::vector<double>& upperBounds) override {
        impl.uniformMutation(chromosome, pm, lowerBounds, upperBounds);
    }
    std::string getName() const override { return "uniform"; }
};

class GaussianMutationWrapper : public IMutationOperator {
private:
    MutationOperators impl;
public:
    void mutate(std::vector<double>& chromosome, double pm,
               const std::vector<double>& lowerBounds,
               const std::vector<double>& upperBounds) override {
        double sigma = 0.1; // Default sigma
        impl.gaussianMutation(chromosome, pm, sigma, lowerBounds, upperBounds);
    }
    std::string getName() const override { return "gaussian"; }
};

class TournamentSelectionWrapper : public ISelectionOperator {
private:
    int tournament_size;
public:
    TournamentSelectionWrapper(int size = 3) : tournament_size(size) {}
    
    Individual select(const std::vector<Individual>& population, std::mt19937& rng) override {
        if (population.empty()) return Individual();
        
        // Convert to non-const for the existing function interface
        std::vector<Individual> pop_copy = population;
        
        // Use the existing TournamentSelection function
        auto indices = TournamentSelection(pop_copy, tournament_size);
        
        if (indices.empty()) return Individual();
        
        // Return the selected individual
        return population[indices[0]];
    }
    std::string getName() const override { return "tournament"; }
};

class RouletteWheelSelectionWrapper : public ISelectionOperator {
public:
    Individual select(const std::vector<Individual>& population, std::mt19937& rng) override {
        if (population.empty()) return Individual();
        
        // Convert to non-const for the existing function interface
        std::vector<Individual> pop_copy = population;
        
        // Use the existing RouletteWheelSelection function
        auto indices = RouletteWheelSelection(pop_copy, 1);
        
        if (indices.empty()) return Individual();
        
        // Return the selected individual
        return population[indices[0]];
    }
    std::string getName() const override { return "roulette"; }
};

class RankSelectionWrapper : public ISelectionOperator {
public:
    Individual select(const std::vector<Individual>& population, std::mt19937& rng) override {
        if (population.empty()) return Individual();
        
        // Convert to non-const for the existing function interface
        std::vector<Individual> pop_copy = population;
        
        // Use the existing RankSelection function
        auto indices = RankSelection(pop_copy, 1);
        
        if (indices.empty()) return Individual();
        
        // Return the selected individual
        return population[indices[0]];
    }
    std::string getName() const override { return "rank"; }
};

// Operator factory system
class OperatorFactory {
private:
    std::map<std::string, std::function<std::unique_ptr<ICrossoverOperator>()>> crossover_factories;
    std::map<std::string, std::function<std::unique_ptr<IMutationOperator>()>> mutation_factories;
    std::map<std::string, std::function<std::unique_ptr<ISelectionOperator>()>> selection_factories;

public:
    OperatorFactory() {
        // Register crossover operators
        crossover_factories["two_point"] = []() { return std::make_unique<TwoPointCrossoverWrapper>(); };
        crossover_factories["one_point"] = []() { return std::make_unique<OnePointCrossoverWrapper>(); };
        crossover_factories["uniform"] = []() { return std::make_unique<UniformCrossoverWrapper>(); };
        crossover_factories["blend"] = []() { return std::make_unique<BlendCrossoverWrapper>(); };
        
        // Register mutation operators
        mutation_factories["uniform"] = []() { return std::make_unique<UniformMutationWrapper>(); };
        mutation_factories["gaussian"] = []() { return std::make_unique<GaussianMutationWrapper>(); };
        
        // Register selection operators
        selection_factories["tournament"] = []() { return std::make_unique<TournamentSelectionWrapper>(3); };
        selection_factories["roulette"] = []() { return std::make_unique<RouletteWheelSelectionWrapper>(); };
        selection_factories["rank"] = []() { return std::make_unique<RankSelectionWrapper>(); };
    }

    std::unique_ptr<ICrossoverOperator> createCrossover(const std::string& name) {
        auto it = crossover_factories.find(name);
        if (it != crossover_factories.end()) {
            return it->second();
        }
        std::cerr << "Unknown crossover operator: " << name << ". Using default two_point." << std::endl;
        return std::make_unique<TwoPointCrossoverWrapper>();
    }

    std::unique_ptr<IMutationOperator> createMutation(const std::string& name) {
        auto it = mutation_factories.find(name);
        if (it != mutation_factories.end()) {
            return it->second();
        }
        std::cerr << "Unknown mutation operator: " << name << ". Using default uniform." << std::endl;
        return std::make_unique<UniformMutationWrapper>();
    }

    std::unique_ptr<ISelectionOperator> createSelection(const std::string& name) {
        auto it = selection_factories.find(name);
        if (it != selection_factories.end()) {
            return it->second();
        }
        std::cerr << "Unknown selection operator: " << name << ". Using default tournament." << std::endl;
        return std::make_unique<TournamentSelectionWrapper>();
    }

    std::vector<std::string> getAvailableCrossovers() {
        std::vector<std::string> names;
        for (const auto& pair : crossover_factories) {
            names.push_back(pair.first);
        }
        return names;
    }

    std::vector<std::string> getAvailableMutations() {
        std::vector<std::string> names;
        for (const auto& pair : mutation_factories) {
            names.push_back(pair.first);
        }
        return names;
    }

    std::vector<std::string> getAvailableSelections() {
        std::vector<std::string> names;
        for (const auto& pair : selection_factories) {
            names.push_back(pair.first);
        }
        return names;
    }
};

// GA Context structure with operator factory integration
struct GAContext {
    // GA Parameters
    int population_size;
    int generations;
    int num_inputs;
    int num_outputs;
    double mutation_rate;
    double crossover_rate;
    double elite_ratio;
    
    // Current state
    int current_generation;
    std::vector<std::vector<double>> population;
    std::vector<double> fitness_values;
    std::vector<double> current_inputs;
    std::vector<double> current_outputs;
    std::vector<double> best_individual;
    double best_fitness;
    
    // Bounds
    std::vector<double> lower_bounds;
    std::vector<double> upper_bounds;
    
    // GA Operators (using your implementations)
    std::unique_ptr<ICrossoverOperator> crossover_op;
    std::unique_ptr<IMutationOperator> mutation_op;
    std::unique_ptr<ISelectionOperator> selection_op;
    
    // Operator names for runtime switching
    std::string crossover_name;
    std::string mutation_name;
    std::string selection_name;
    
    // Factory for creating operators
    OperatorFactory factory;
    
    // Fitness function
#ifdef INCLUDE_FITNESS_FUNCTION
    std::unique_ptr<FitnessFunction> fitness_function;
#else
    std::unique_ptr<SimpleFitnessFunction> fitness_function;
#endif
    
    // Random number generator
    std::mt19937 rng;
    std::uniform_real_distribution<double> uniform_dist;
    
    GAContext(int pop_size, int gen, int inputs, int outputs, 
              double mut_rate, double cross_rate)
        : population_size(pop_size), generations(gen), 
          num_inputs(inputs), num_outputs(outputs),
          mutation_rate(mut_rate), crossover_rate(cross_rate),
          elite_ratio(0.1), current_generation(0), best_fitness(-1e10),
          crossover_name("two_point"), mutation_name("uniform"), selection_name("tournament"),
          rng(std::random_device{}()), uniform_dist(0.0, 1.0) {
        
        // Initialize bounds (default: [-10, 10])
        lower_bounds.resize(inputs + outputs, -10.0);
        upper_bounds.resize(inputs + outputs, 10.0);
        
        // Create operators using factory
        configureOperators();
        
        // Initialize fitness function
#ifdef INCLUDE_FITNESS_FUNCTION
        fitness_function = std::make_unique<RastriginFunction>();
#else
        fitness_function = std::make_unique<SimpleFitnessFunction>();
#endif
        
        // Initialize population
        initializePopulation();
        
        std::cout << "Unified GA Library initialized:" << std::endl;
        std::cout << "  Population: " << pop_size << ", Generations: " << gen << std::endl;
        std::cout << "  Inputs: " << inputs << ", Outputs: " << outputs << std::endl;
        std::cout << "  Crossover: " << crossover_name << ", Mutation: " << mutation_name 
                  << ", Selection: " << selection_name << std::endl;
        std::cout << "  Elite ratio: " << elite_ratio << std::endl;
    }
    
    void configureOperators() {
        crossover_op = factory.createCrossover(crossover_name);
        mutation_op = factory.createMutation(mutation_name);
        selection_op = factory.createSelection(selection_name);
        
        std::cout << "Operators configured: " 
                  << crossover_op->getName() << "/" 
                  << mutation_op->getName() << "/" 
                  << selection_op->getName() << std::endl;
    }
    
    void initializePopulation() {
        population.resize(population_size);
        fitness_values.resize(population_size);
        
        for (int i = 0; i < population_size; ++i) {
            population[i].resize(num_inputs + num_outputs);
            
            // Random initialization within bounds
            for (int j = 0; j < num_inputs + num_outputs; ++j) {
                population[i][j] = lower_bounds[j] + 
                    uniform_dist(rng) * (upper_bounds[j] - lower_bounds[j]);
            }
        }
        
        // Initialize best individual
        best_individual.resize(num_inputs + num_outputs);
        current_inputs.resize(num_inputs);
        current_outputs.resize(num_outputs);
        
        std::cout << "Population initialized with " << population_size << " individuals" << std::endl;
    }
    
    void evaluateFitness() {
        for (int i = 0; i < population_size; ++i) {
            // Evaluate fitness using fitness function
            fitness_values[i] = fitness_function->evaluate(population[i]);
            
            // Track best individual
            if (fitness_values[i] > best_fitness) {
                best_fitness = fitness_values[i];
                best_individual = population[i];
                
                // Update current outputs (last num_outputs elements)
                for (int j = 0; j < num_outputs; ++j) {
                    current_outputs[j] = best_individual[num_inputs + j];
                }
            }
        }
    }
    
    std::vector<int> selectElite() {
        int elite_count = static_cast<int>(population_size * elite_ratio);
        if (elite_count < 1) elite_count = 1;
        
        // Get indices sorted by fitness (descending)
        std::vector<int> indices(population_size);
        std::iota(indices.begin(), indices.end(), 0);
        
        std::sort(indices.begin(), indices.end(), 
                  [this](int a, int b) { return fitness_values[a] > fitness_values[b]; });
        
        return std::vector<int>(indices.begin(), indices.begin() + elite_count);
    }
    
    void performCrossover(std::vector<std::vector<double>>& new_population, int& offspring_count) {
        while (offspring_count < population_size) {
            if (uniform_dist(rng) < crossover_rate) {
                // Select two parents using selection operator
                std::vector<Individual> temp_pop;
                for (int i = 0; i < population_size; ++i) {
                    Individual temp_ind;
                    temp_ind.genes = population[i];
                    temp_ind.fitness = fitness_values[i];
                    temp_pop.push_back(temp_ind);
                }
                
                Individual parent1 = selection_op->select(temp_pop, rng);
                Individual parent2 = selection_op->select(temp_pop, rng);
                
                // Perform crossover using selected operator
                auto offspring = crossover_op->crossover(parent1.genes, parent2.genes);
                
                // Add offspring to new population
                if (offspring_count < population_size) {
                    new_population[offspring_count] = offspring.first;
                    offspring_count++;
                }
                if (offspring_count < population_size) {
                    new_population[offspring_count] = offspring.second;
                    offspring_count++;
                }
            } else {
                // No crossover, just copy parents
                if (offspring_count < population_size) {
                    int parent_idx = static_cast<int>(uniform_dist(rng) * population_size);
                    new_population[offspring_count] = population[parent_idx];
                    offspring_count++;
                }
            }
        }
    }
    
    void evolveGeneration() {
        // Evaluate current population
        evaluateFitness();
        
        // Select elite individuals (ELITISM)
        std::vector<int> elite_indices = selectElite();
        
        // Create new population
        std::vector<std::vector<double>> new_population(population_size);
        int offspring_count = 0;
        
        // Copy elite individuals (preserve best solutions)
        for (int elite_idx : elite_indices) {
            if (offspring_count < population_size) {
                new_population[offspring_count] = population[elite_idx];
                offspring_count++;
            }
        }
        
        // Fill rest with crossover
        performCrossover(new_population, offspring_count);
        
        // Apply mutation (skip elite individuals to preserve them)
        for (int i = elite_indices.size(); i < population_size; ++i) {
            mutation_op->mutate(new_population[i], mutation_rate, lower_bounds, upper_bounds);
        }
        
        // Replace old population
        population = std::move(new_population);
        current_generation++;
    }
    
    void updateInputs(const std::vector<double>& inputs) {
        current_inputs = inputs;
        
        // Update population to include current inputs
        for (auto& individual : population) {
            for (int i = 0; i < num_inputs && i < static_cast<int>(inputs.size()); ++i) {
                individual[i] = inputs[i];
            }
        }
    }
    
    void printStatistics() const {
        std::cout << "Generation " << current_generation 
                  << ": Best=" << std::fixed << std::setprecision(6) << best_fitness;
        
        // Calculate average fitness
        double avg_fitness = 0.0;
        for (double fit : fitness_values) {
            avg_fitness += fit;
        }
        avg_fitness /= population_size;
        
        std::cout << ", Avg=" << std::fixed << std::setprecision(6) << avg_fitness 
                  << " [" << crossover_op->getName() << "/" << mutation_op->getName() 
                  << "/" << selection_op->getName() << "]" << std::endl;
    }
};

extern "C" {

GAContext* ga_init(int population_size, int num_generations, 
                   int num_inputs, int num_outputs,
                   double mutation_rate, double crossover_rate) {
    try {
        return new GAContext(population_size, num_generations, num_inputs, num_outputs,
                           mutation_rate, crossover_rate);
    } catch (const std::exception& e) {
        std::cerr << "GA initialization failed: " << e.what() << std::endl;
        return nullptr;
    }
}

int ga_run(GAContext* ctx, int max_iterations) {
    if (!ctx) return 0;
    
    try {
        for (int i = 0; i < max_iterations && ctx->current_generation < ctx->generations; ++i) {
            ctx->evolveGeneration();
            
            // Print progress
            if (i % 5 == 0 || i == max_iterations - 1) {
                ctx->printStatistics();
            }
        }
        
        return ctx->current_generation;
    } catch (const std::exception& e) {
        std::cerr << "GA run failed: " << e.what() << std::endl;
        return 0;
    }
}

double ga_get_best(GAContext* ctx) {
    if (!ctx) return 0.0;
    return ctx->best_fitness;
}

void ga_set_inputs(GAContext* ctx, const double* inputs, int count) {
    if (!ctx || !inputs) return;
    
    std::vector<double> input_vec(inputs, inputs + std::min(count, ctx->num_inputs));
    ctx->updateInputs(input_vec);
    
    std::cout << "GA inputs updated: ";
    for (int i = 0; i < std::min(count, 3); ++i) {
        std::cout << std::fixed << std::setprecision(6) << inputs[i] << " ";
    }
    if (count > 3) std::cout << "...";
    std::cout << std::endl;
}

void ga_get_outputs(GAContext* ctx, double* outputs, int count) {
    if (!ctx || !outputs) return;
    
    int copy_count = std::min(count, ctx->num_outputs);
    for (int i = 0; i < copy_count; ++i) {
        outputs[i] = (i < static_cast<int>(ctx->current_outputs.size())) ? ctx->current_outputs[i] : 0.0;
    }
    
    std::cout << "GA outputs retrieved: ";
    for (int i = 0; i < std::min(copy_count, 3); ++i) {
        std::cout << std::fixed << std::setprecision(6) << outputs[i] << " ";
    }
    if (copy_count > 3) std::cout << "...";
    std::cout << std::endl;
}

void ga_cleanup(GAContext* ctx) {
    if (ctx) {
        std::cout << "GA cleanup: Final generation=" << ctx->current_generation 
                 << ", Best fitness=" << std::fixed << std::setprecision(6) << ctx->best_fitness << std::endl;
        delete ctx;
    }
}

// Runtime operator configuration functions
void ga_set_operators(GAContext* ctx, const char* crossover, const char* mutation, const char* selection) {
    if (!ctx) return;
    
    bool changed = false;
    
    if (crossover && std::string(crossover) != ctx->crossover_name) {
        ctx->crossover_name = crossover;
        changed = true;
    }
    
    if (mutation && std::string(mutation) != ctx->mutation_name) {
        ctx->mutation_name = mutation;
        changed = true;
    }
    
    if (selection && std::string(selection) != ctx->selection_name) {
        ctx->selection_name = selection;
        changed = true;
    }
    
    if (changed) {
        ctx->configureOperators();
        std::cout << "GA operators updated to: " << ctx->crossover_name 
                  << "/" << ctx->mutation_name << "/" << ctx->selection_name << std::endl;
    }
}

// List available operators
void ga_list_operators(GAContext* ctx) {
    if (!ctx) return;
    
    auto crossovers = ctx->factory.getAvailableCrossovers();
    auto mutations = ctx->factory.getAvailableMutations();
    auto selections = ctx->factory.getAvailableSelections();
    
    std::cout << "Available operators:" << std::endl;
    std::cout << "  Crossover: ";
    for (const auto& op : crossovers) std::cout << op << " ";
    std::cout << std::endl;
    
    std::cout << "  Mutation: ";
    for (const auto& op : mutations) std::cout << op << " ";
    std::cout << std::endl;
    
    std::cout << "  Selection: ";
    for (const auto& op : selections) std::cout << op << " ";
    std::cout << std::endl;
}

// Additional utility functions
void ga_set_bounds(GAContext* ctx, const double* lower, const double* upper, int count) {
    if (!ctx || !lower || !upper) return;
    
    int total_vars = ctx->num_inputs + ctx->num_outputs;
    for (int i = 0; i < count && i < total_vars; ++i) {
        ctx->lower_bounds[i] = lower[i];
        ctx->upper_bounds[i] = upper[i];
    }
    std::cout << "GA bounds updated for " << count << " variables" << std::endl;
}

void ga_set_elite_ratio(GAContext* ctx, double ratio) {
    if (ctx && ratio >= 0.0 && ratio <= 1.0) {
        ctx->elite_ratio = ratio;
        std::cout << "Elite ratio set to " << ratio << std::endl;
    }
}

void ga_set_mutation_rate(GAContext* ctx, double rate) {
    if (ctx && rate >= 0.0 && rate <= 1.0) {
        ctx->mutation_rate = rate;
        std::cout << "Mutation rate set to " << rate << std::endl;
    }
}

void ga_set_crossover_rate(GAContext* ctx, double rate) {
    if (ctx && rate >= 0.0 && rate <= 1.0) {
        ctx->crossover_rate = rate;
        std::cout << "Crossover rate set to " << rate << std::endl;
    }
}

int ga_get_generation(GAContext* ctx) {
    return ctx ? ctx->current_generation : 0;
}

void ga_get_best_individual(GAContext* ctx, double* individual, int count) {
    if (!ctx || !individual) return;
    
    int copy_count = std::min(count, static_cast<int>(ctx->best_individual.size()));
    for (int i = 0; i < copy_count; ++i) {
        individual[i] = ctx->best_individual[i];
    }
}

} // extern "C"