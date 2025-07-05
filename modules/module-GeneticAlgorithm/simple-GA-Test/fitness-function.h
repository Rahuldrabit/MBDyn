#ifndef FITNESS_FUNCTION_H
#define FITNESS_FUNCTION_H

#include <vector>
#include <cmath>

// Generate random fitness value for testing
double generateRandomFitness();

// Benchmark optimization functions for GA testing
// All functions are minimization problems (lower values are better)

/**
 * @brief Rastrigin function - highly multimodal with many local optima
 * Global minimum: f(0,...,0) = 0
 * Search domain: typically [-5.12, 5.12]^n
 */
double rastriginFunction(const std::vector<double>& x);

/**
 * @brief Ackley function - has one global minimum and many local minima
 * Global minimum: f(0,...,0) = 0
 * Search domain: typically [-32.768, 32.768]^n
 */
double ackleyFunction(const std::vector<double>& x);

/**
 * @brief Schwefel function - deceptive function with global optimum far from local optima
 * Global minimum: f(420.9687,...,420.9687) ≈ 0
 * Search domain: typically [-500, 500]^n
 */
double schwefelFunction(const std::vector<double>& x);

// Wrapper functions that convert minimization to maximization for GA
double rastriginFitness(const std::vector<double>& x);
double ackleyFitness(const std::vector<double>& x);
double schwefelFitness(const std::vector<double>& x);

#endif // FITNESS_FUNCTION_H
