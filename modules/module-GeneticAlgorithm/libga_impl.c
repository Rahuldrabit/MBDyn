/*
 * Example implementation of libga.so - Genetic Algorithm Library
 * Compile with: gcc -shared -fPIC -o libga.so libga_impl.c -lm
 */

#include "libga_interface.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

static int seeded = 0;

void generate_population(double* population, int popSize, int genomeLen) {
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
    
    for (int i = 0; i < popSize * genomeLen; i++) {
        // Generate random values between 0 and 1
        population[i] = (double)rand() / RAND_MAX;
    }
}

double fitness(const double* genome, int genomeLen) {
    // Example fitness function - minimize sum of squares
    // Higher fitness = better, so return negative of sum of squares
    double sum = 0.0;
    for (int i = 0; i < genomeLen; i++) {
        sum += genome[i] * genome[i];
    }
    return -sum;  // Negative because we want to minimize
}

void crossover(const double* parent1, const double* parent2,
               double* child1, double* child2,
               int genomeLen, double crossoverRate) {
    
    if ((double)rand() / RAND_MAX > crossoverRate) {
        // No crossover - copy parents to children
        memcpy(child1, parent1, genomeLen * sizeof(double));
        memcpy(child2, parent2, genomeLen * sizeof(double));
        return;
    }
    
    // Single-point crossover
    int crossPoint = rand() % genomeLen;
    
    for (int i = 0; i < genomeLen; i++) {
        if (i < crossPoint) {
            child1[i] = parent1[i];
            child2[i] = parent2[i];
        } else {
            child1[i] = parent2[i];
            child2[i] = parent1[i];
        }
    }
}

void mutate(double* genome, int genomeLen, double mutationRate, double mutationStrength) {
    for (int i = 0; i < genomeLen; i++) {
        if ((double)rand() / RAND_MAX < mutationRate) {
            // Gaussian mutation
            double mutation = mutationStrength * (2.0 * (double)rand() / RAND_MAX - 1.0);
            genome[i] += mutation;
            
            // Keep within bounds [0, 1]
            if (genome[i] < 0.0) genome[i] = 0.0;
            if (genome[i] > 1.0) genome[i] = 1.0;
        }
    }
}

int tournament_selection(const double* population, const double* fitness_values,
                        int popSize, int genomeLen, int tournamentSize) {
    int best_idx = rand() % popSize;
    double best_fitness = fitness_values[best_idx];
    
    for (int i = 1; i < tournamentSize; i++) {
        int idx = rand() % popSize;
        if (fitness_values[idx] > best_fitness) {
            best_idx = idx;
            best_fitness = fitness_values[idx];
        }
    }
    
    return best_idx;
}
