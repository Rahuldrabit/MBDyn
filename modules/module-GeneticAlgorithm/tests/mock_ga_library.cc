/* Mock GA Library Implementation for Testing
 * 
 * This provides a simple mock implementation of the GA library
 * that your module expects to load via dlopen. This allows testing
 * the module logic without requiring the actual GA library.
 */

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <random>

// Mock GA context structure
struct MockGAContext {
    int population_size;
    int generations;
    double best_fitness;
    bool has_run;
    int current_generation;
    std::mt19937 rng;
    
    MockGAContext(int pop, int gen) 
        : population_size(pop), generations(gen), best_fitness(0.0), 
          has_run(false), current_generation(0), rng(42) {}
};

// Mock GA library functions - these match the function signatures expected by the module
extern "C" {

void* mock_ga_init(int pop_size, int generations) {
    if (pop_size <= 0 || generations <= 0) {
        return nullptr;
    }
    
    MockGAContext* ctx = new MockGAContext(pop_size, generations);
    std::cout << "Mock GA initialized with population=" << pop_size 
              << ", generations=" << generations << std::endl;
    return static_cast<void*>(ctx);
}

void mock_ga_run(void* ga_ctx) {
    if (!ga_ctx) {
        return;
    }
    
    MockGAContext* ctx = static_cast<MockGAContext*>(ga_ctx);
    
    // Simulate GA run - fitness generally improves over generations
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    if (!ctx->has_run) {
        // First run - start with random fitness
        ctx->best_fitness = dist(ctx->rng) * 0.5;  // Start with lower fitness
        ctx->has_run = true;
    } else {
        // Subsequent runs - gradually improve fitness
        double improvement = dist(ctx->rng) * 0.1;  // Small random improvement
        ctx->best_fitness = std::min(1.0, ctx->best_fitness + improvement);
    }
    
    ctx->current_generation++;
    
    std::cout << "Mock GA run completed. Generation=" << ctx->current_generation
              << ", Best fitness=" << ctx->best_fitness << std::endl;
}

double mock_ga_get_best(void* ga_ctx) {
    if (!ga_ctx) {
        return 0.0;
    }
    
    MockGAContext* ctx = static_cast<MockGAContext*>(ga_ctx);
    return ctx->best_fitness;
}

void mock_ga_cleanup(void* ga_ctx) {
    if (ga_ctx) {
        MockGAContext* ctx = static_cast<MockGAContext*>(ga_ctx);
        delete ctx;
    }
}

// Additional mock functions that might be useful for extended testing
int mock_ga_get_generation(void* ga_ctx) {
    if (!ga_ctx) {
        return 0;
    }
    MockGAContext* ctx = static_cast<MockGAContext*>(ga_ctx);
    return ctx->current_generation;
}

int mock_ga_get_population_size(void* ga_ctx) {
    if (!ga_ctx) {
        return 0;
    }
    MockGAContext* ctx = static_cast<MockGAContext*>(ga_ctx);
    return ctx->population_size;
}

double* mock_ga_get_population_fitness(void* ga_ctx, int* size) {
    if (!ga_ctx || !size) {
        if (size) *size = 0;
        return nullptr;
    }
    
    MockGAContext* ctx = static_cast<MockGAContext*>(ga_ctx);
    *size = ctx->population_size;
    
    // Generate mock fitness values for entire population
    double* fitness_array = new double[ctx->population_size];
    std::uniform_real_distribution<double> dist(0.0, ctx->best_fitness);
    
    for (int i = 0; i < ctx->population_size; ++i) {
        fitness_array[i] = dist(ctx->rng);
    }
    
    // Ensure at least one individual has the best fitness
    if (ctx->population_size > 0) {
        fitness_array[0] = ctx->best_fitness;
    }
    
    return fitness_array;
}

void mock_ga_free_fitness_array(double* fitness_array) {
    delete[] fitness_array;
}

// Mock parameter setting functions
void mock_ga_set_crossover_rate(void* ga_ctx, double rate) {
    if (ga_ctx) {
        std::cout << "Mock GA: Set crossover rate to " << rate << std::endl;
    }
}

void mock_ga_set_mutation_rate(void* ga_ctx, double rate) {
    if (ga_ctx) {
        std::cout << "Mock GA: Set mutation rate to " << rate << std::endl;
    }
}

void mock_ga_set_selection_method(void* ga_ctx, const char* method) {
    if (ga_ctx && method) {
        std::cout << "Mock GA: Set selection method to " << method << std::endl;
    }
}

// Fitness function simulation
double mock_fitness_function(const double* genome, int genome_length) {
    if (!genome || genome_length <= 0) {
        return 0.0;
    }
    
    // Simple mock fitness: sum of genome values normalized by length
    double sum = 0.0;
    for (int i = 0; i < genome_length; ++i) {
        sum += genome[i];
    }
    return sum / genome_length;  // Average value
}

// Mock constraint checking
bool mock_constraint_check(const double* genome, int genome_length) {
    if (!genome || genome_length <= 0) {
        return false;
    }
    
    // Simple constraint: all values should be between 0 and 1
    for (int i = 0; i < genome_length; ++i) {
        if (genome[i] < 0.0 || genome[i] > 1.0) {
            return false;
        }
    }
    return true;
}

} // extern "C"