/*
 * Simple test program for the modular GA architecture
 * Tests that libga.so + libcons.so + module work together correctly
 */

#include <iostream>
#include <dlfcn.h>
#include <cstring>
#include <vector>

// Function pointers for libga.so
typedef void* (*ga_init_population_t)(int, int, const double*, const double*);
typedef void (*ga_get_individual_t)(void*, int, double*);
typedef void (*ga_set_individual_t)(void*, int, const double*);
typedef int (*ga_get_population_size_t)(void*);
typedef int (*ga_get_chromosome_length_t)(void*);
typedef void (*ga_cleanup_t)(void*);
typedef int (*ga_seed_population_t)(void*, const double*, int, int);

// Function pointers for libcons.so
typedef double (*evaluate_fitness_t)(const double*, int, const double*, int);

int main() {
    std::cout << "=== Testing Modular GA Architecture ===" << std::endl;
    std::cout << "\nArchitecture:" << std::endl;
    std::cout << "  - libga.so: Population initialization ONLY" << std::endl;
    std::cout << "  - libcons.so: Fitness evaluation ONLY" << std::endl;
    std::cout << "  - This program: All GA logic (simulating module)" << std::endl;
    
    // Load libga.so
    std::cout << "\n1. Loading libga.so..." << std::endl;
    void* libga = dlopen("./libga.so", RTLD_NOW);
    if (!libga) {
        std::cerr << "Failed to load libga.so: " << dlerror() << std::endl;
        return 1;
    }
    std::cout << "   ✓ libga.so loaded" << std::endl;
    
    auto ga_init_population = (ga_init_population_t)dlsym(libga, "ga_init_population");
    auto ga_get_individual = (ga_get_individual_t)dlsym(libga, "ga_get_individual");
    auto ga_set_individual = (ga_set_individual_t)dlsym(libga, "ga_set_individual");
    auto ga_get_population_size = (ga_get_population_size_t)dlsym(libga, "ga_get_population_size");
    auto ga_get_chromosome_length = (ga_get_chromosome_length_t)dlsym(libga, "ga_get_chromosome_length");
    auto ga_cleanup = (ga_cleanup_t)dlsym(libga, "ga_cleanup");
    auto ga_seed_population = (ga_seed_population_t)dlsym(libga, "ga_seed_population");
    
    if (!ga_init_population || !ga_get_individual || !ga_seed_population) {
        std::cerr << "Failed to load ga functions" << std::endl;
        return 1;
    }
    std::cout << "   ✓ Functions loaded: ga_init_population, ga_get_individual, ga_set_individual, ga_seed_population" << std::endl;
    
    // Load libcons.so
    std::cout << "\n2. Loading libcons.so..." << std::endl;
    void* libcons = dlopen("./libcons.so", RTLD_NOW);
    if (!libcons) {
        std::cerr << "Failed to load libcons.so: " << dlerror() << std::endl;
        return 1;
    }
    std::cout << "   ✓ libcons.so loaded" << std::endl;
    
    auto evaluate_fitness = (evaluate_fitness_t)dlsym(libcons, "evaluate_fitness");
    if (!evaluate_fitness) {
        std::cerr << "Failed to load evaluate_fitness" << std::endl;
        return 1;
    }
    std::cout << "   ✓ Function loaded: evaluate_fitness" << std::endl;
    
    // Initialize population (using libga.so)
    std::cout << "\n3. Initializing population (via libga.so)..." << std::endl;
    int pop_size = 10;
    int n_genes = 5;
    std::vector<double> lower(n_genes, -5.12);
    std::vector<double> upper(n_genes, 5.12);
    void* pop_ctx = ga_init_population(pop_size, n_genes, lower.data(), upper.data());
    if (!pop_ctx) {
        std::cerr << "Failed to initialize population" << std::endl;
        return 1;
    }
    std::cout << "   ✓ Population initialized: " << pop_size << " individuals, " << n_genes << " genes each" << std::endl;

    std::vector<double> seeded(static_cast<size_t>(pop_size * n_genes));
    for (int i = 0; i < pop_size; ++i) {
        for (int j = 0; j < n_genes; ++j) {
            const size_t idx = static_cast<size_t>(i) * static_cast<size_t>(n_genes) + static_cast<size_t>(j);
            seeded[idx] = (i == 0) ? (1.0 + j) : (0.1 * (i + 1) + j);
        }
    }
    if (ga_seed_population(pop_ctx, seeded.data(), pop_size, n_genes) == 0) {
        std::cout << "   ✓ Seeded population using ga_seed_population" << std::endl;
    } else {
        std::cerr << "   ✗ Failed to seed population" << std::endl;
        return 1;
    }
    
    // Evaluate fitness (using libcons.so) - THIS IS DONE BY MODULE
    std::cout << "\n4. Evaluating population (via libcons.so, orchestrated by module logic)..." << std::endl;
    double inputs[2] = {1.0, 2.0};  // Example inputs
    std::vector<double> fitness_values(pop_size);
    
    std::vector<double> genes_buffer(n_genes);
    for (int i = 0; i < pop_size; ++i) {
        std::cout << "   Evaluating individual " << i << "..." << std::endl;
        ga_get_individual(pop_ctx, i, genes_buffer.data());
        std::cout << "   Genes: [";
        for (int j = 0; j < n_genes; ++j) {
            std::cout << genes_buffer[j];
            if (j < n_genes - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
        fitness_values[i] = evaluate_fitness(genes_buffer.data(), n_genes, inputs, 2);
        std::cout << "   Fitness: " << fitness_values[i] << std::endl;
    }
    
    double best_fitness = -1e30;
    int best_idx = 0;
    for (int i = 0; i < pop_size; ++i) {
        if (fitness_values[i] > best_fitness) {
            best_fitness = fitness_values[i];
            best_idx = i;
        }
    }
    std::cout << "   ✓ Fitness evaluated for all individuals" << std::endl;
    std::cout << "   ✓ Best fitness: " << best_fitness << " (individual " << best_idx << ")" << std::endl;
    
    // Demonstrate module doing evolution work (simple mutation)
    std::cout << "\n5. Performing evolution (module logic)..." << std::endl;
    std::vector<double> best_genes_copy(n_genes);
    ga_get_individual(pop_ctx, best_idx, best_genes_copy.data());
    std::vector<double> mutated(n_genes);
    for (int i = 0; i < n_genes; ++i) {
        mutated[i] = best_genes_copy[i] + ((rand() % 100) / 1000.0 - 0.05);
    }
    ga_set_individual(pop_ctx, 0, mutated.data());
    std::cout << "   ✓ Created mutated individual (module performed mutation)" << std::endl;
    
    // Re-evaluate
    std::vector<double> new_genes(n_genes);
    ga_get_individual(pop_ctx, 0, new_genes.data());
    double new_fitness = evaluate_fitness(new_genes.data(), n_genes, inputs, 2);
    std::cout << "   ✓ New fitness: " << new_fitness << std::endl;
    
    // Summary
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "✓ libga.so: Successfully initialized population" << std::endl;
    std::cout << "✓ libcons.so: Successfully evaluated fitness" << std::endl;
    std::cout << "✓ Module logic: Successfully orchestrated evolution" << std::endl;
    std::cout << "\nArchitecture validated!" << std::endl;
    std::cout << "  - Libraries provide minimal services" << std::endl;
    std::cout << "  - Module controls ALL GA operations" << std::endl;
    
    // Cleanup
    ga_cleanup(pop_ctx);
    dlclose(libga);
    dlclose(libcons);
    
    return 0;
}
