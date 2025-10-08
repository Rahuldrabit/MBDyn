/*
 * Minimal GA library - ONLY initializes population
 * All GA logic (selection, crossover, mutation) happens in the UDE module
 */

#include <vector>
#include <random>
#include <iostream>

struct PopulationContext {
    int population_size;
    int chromosome_length;
    std::vector<std::vector<double>> population;
    std::vector<double> lower_bounds;
    std::vector<double> upper_bounds;
    std::mt19937 rng;
};

extern "C" {

// Initialize population with random individuals
void* ga_init_population(int pop_size, int chrom_len, 
                         const double* lower, const double* upper) {
    PopulationContext* ctx = new PopulationContext();
    ctx->population_size = pop_size;
    ctx->chromosome_length = chrom_len;
    ctx->rng.seed(std::random_device{}());
    
    // Store bounds
    ctx->lower_bounds.assign(lower, lower + chrom_len);
    ctx->upper_bounds.assign(upper, upper + chrom_len);
    
    // Initialize population with random values
    ctx->population.resize(pop_size);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (int i = 0; i < pop_size; ++i) {
        ctx->population[i].resize(chrom_len);
        for (int j = 0; j < chrom_len; ++j) {
            double range = upper[j] - lower[j];
            ctx->population[i][j] = lower[j] + dist(ctx->rng) * range;
        }
    }
    
    std::cout << "Population initialized: " << pop_size << " individuals, "
              << chrom_len << " genes each" << std::endl;
    
    return ctx;
}

// Get population data (read-only access)
void ga_get_population(void* ctx, double* pop_data) {
    PopulationContext* pc = static_cast<PopulationContext*>(ctx);
    if (!pc || !pop_data) return;
    
    int idx = 0;
    for (const auto& individual : pc->population) {
        for (double gene : individual) {
            pop_data[idx++] = gene;
        }
    }
}

// Set population data (after module does evolution)
void ga_set_population(void* ctx, const double* pop_data) {
    PopulationContext* pc = static_cast<PopulationContext*>(ctx);
    if (!pc || !pop_data) return;
    
    int idx = 0;
    for (auto& individual : pc->population) {
        for (double& gene : individual) {
            gene = pop_data[idx++];
        }
    }
}

// Get individual
void ga_get_individual(void* ctx, int index, double* individual) {
    PopulationContext* pc = static_cast<PopulationContext*>(ctx);
    if (!pc || index < 0 || index >= pc->population_size || !individual) return;
    
    for (int j = 0; j < pc->chromosome_length; ++j) {
        individual[j] = pc->population[index][j];
    }
}

// Set individual
void ga_set_individual(void* ctx, int index, const double* individual) {
    PopulationContext* pc = static_cast<PopulationContext*>(ctx);
    if (!pc || index < 0 || index >= pc->population_size || !individual) return;
    
    for (int j = 0; j < pc->chromosome_length; ++j) {
        pc->population[index][j] = individual[j];
    }
}

// Get bounds
void ga_get_bounds(void* ctx, double* lower, double* upper) {
    PopulationContext* pc = static_cast<PopulationContext*>(ctx);
    if (!pc || !lower || !upper) return;
    
    for (int j = 0; j < pc->chromosome_length; ++j) {
        lower[j] = pc->lower_bounds[j];
        upper[j] = pc->upper_bounds[j];
    }
}

// Get population size
int ga_get_population_size(void* ctx) {
    PopulationContext* pc = static_cast<PopulationContext*>(ctx);
    return pc ? pc->population_size : 0;
}

// Get chromosome length
int ga_get_chromosome_length(void* ctx) {
    PopulationContext* pc = static_cast<PopulationContext*>(ctx);
    return pc ? pc->chromosome_length : 0;
}

// Cleanup
void ga_cleanup(void* ctx) {
    delete static_cast<PopulationContext*>(ctx);
}

} // extern "C"
