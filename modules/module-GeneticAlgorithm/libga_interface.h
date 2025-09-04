/* 
 * Interface for libga.so - Genetic Algorithm Library
 * This library contains population generation, mutation, crossover, and fitness functions
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Generate initial random population
 * population: output array to fill with random values
 * popSize: number of individuals in population
 * genomeLen: length of each individual's genome (number of parameters)
 */
void generate_population(double* population, int popSize, int genomeLen);

/* Calculate fitness for a single genome
 * genome: array of parameter values (length = genomeLen)
 * genomeLen: number of parameters in genome
 * Returns: fitness value (higher = better)
 */
double fitness(const double* genome, int genomeLen);

/* Perform crossover between two parent genomes
 * parent1, parent2: input parent genomes
 * child1, child2: output child genomes
 * genomeLen: length of genomes
 * crossoverRate: probability of crossover (0.0-1.0)
 */
void crossover(const double* parent1, const double* parent2, 
               double* child1, double* child2, 
               int genomeLen, double crossoverRate);

/* Perform mutation on a genome
 * genome: genome to mutate (modified in place)
 * genomeLen: length of genome
 * mutationRate: probability of mutation (0.0-1.0)
 * mutationStrength: magnitude of mutation
 */
void mutate(double* genome, int genomeLen, double mutationRate, double mutationStrength);

/* Select parents for reproduction using tournament selection
 * population: current population
 * fitness_values: fitness for each individual
 * popSize: population size
 * genomeLen: genome length
 * tournamentSize: number of individuals in tournament
 * Returns: index of selected individual
 */
int tournament_selection(const double* population, const double* fitness_values,
                        int popSize, int genomeLen, int tournamentSize);

#ifdef __cplusplus
}
#endif
