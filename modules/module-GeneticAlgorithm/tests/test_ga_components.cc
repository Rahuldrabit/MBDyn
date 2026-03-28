/* Standalone unit tests for the Genetic Algorithm module components
 *
 * This program tests:
 * 1. Fitness functions (Rastrigin, Ackley, Schwefel)
 * 2. Crossover operators (one-point, two-point, uniform, blend, SBX, arithmetic)
 * 3. Mutation operators (Gaussian, uniform, bit-flip, swap)
 * 4. Selection operators (tournament, roulette)
 * 5. Complete GA runs on benchmark functions
 *
 * Can be compiled and run without MBDyn:
 *   g++ -std=c++17 -O2 -I.. -o test_ga_components test_ga_components.cc \
 *       ../simple-GA-Test/fitness-fuction.cc \
 *       ../crossover/crossover.cc \
 *       ../mutation/mutation.cc \
 *       ../selection-operator/selection-operator.cc
 */

#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <random>
#include <stdexcept>
#include <limits>

#include "simple-GA-Test/fitness-function.h"
#include "crossover/crossover.h"
#include "mutation/mutation.h"
#include "selection-operator/selection-operator.h"

// Test framework macros
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (line " << __LINE__ << ")" << std::endl; \
            tests_failed++; \
        } else { \
            std::cout << "PASS: " << msg << std::endl; \
            tests_passed++; \
        } \
    } while(0)

#define TEST_NEAR(a, b, tol, msg) \
    TEST_ASSERT(std::fabs((a) - (b)) < (tol), msg)

// =============================================================================
// Test fitness functions
// =============================================================================
void testFitnessFunctions() {
    std::cout << "\n=== Testing Fitness Functions ===" << std::endl;

    // Rastrigin: global minimum at origin = 0
    {
        std::vector<double> origin(10, 0.0);
        double val = rastriginFunction(origin);
        TEST_NEAR(val, 0.0, 1e-10, "Rastrigin at origin = 0");

        // Fitness wrapper should give high value near origin
        double fit = rastriginFitness(origin);
        TEST_ASSERT(fit > 0.0, "Rastrigin fitness > 0 at origin");
        TEST_NEAR(fit, 1000.0, 1e-6, "Rastrigin fitness at origin ≈ 1000");
    }

    // Rastrigin: non-zero point gives positive value
    {
        std::vector<double> point = {1.0, 1.0, 1.0};
        double val = rastriginFunction(point);
        TEST_ASSERT(val > 0.0, "Rastrigin function > 0 at non-zero point");
    }

    // Ackley: global minimum at origin = 0
    {
        std::vector<double> origin(5, 0.0);
        double val = ackleyFunction(origin);
        TEST_NEAR(val, 0.0, 1e-10, "Ackley at origin = 0");

        double fit = ackleyFitness(origin);
        TEST_NEAR(fit, 1000.0, 1e-6, "Ackley fitness at origin ≈ 1000");
    }

    // Schwefel: global minimum at ~420.9687 gives near 0
    {
        double opt = 420.9687;
        std::vector<double> optimal = {opt, opt, opt, opt, opt};
        double val = schwefelFunction(optimal);
        TEST_ASSERT(val < 1.0, "Schwefel near optimum < 1");

        double fit = schwefelFitness(optimal);
        TEST_ASSERT(fit > 0.0, "Schwefel fitness > 0");
        TEST_ASSERT(fit < 1001.0, "Schwefel fitness < 1001");
    }
}

// =============================================================================
// Test crossover operators
// =============================================================================
void testCrossoverOperators() {
    std::cout << "\n=== Testing Crossover Operators ===" << std::endl;

    std::mt19937 rng(42);

    // One-point crossover
    {
        OnePointCrossover op;
        RealVector p1 = {1.0, 2.0, 3.0, 4.0, 5.0};
        RealVector p2 = {6.0, 7.0, 8.0, 9.0, 10.0};
        auto [c1, c2] = op.crossover(p1, p2);

        TEST_ASSERT(c1.size() == p1.size(), "One-point crossover: child1 size correct");
        TEST_ASSERT(c2.size() == p2.size(), "One-point crossover: child2 size correct");

        // Children should contain genes from both parents
        bool c1_has_p1 = false, c1_has_p2 = false;
        for (size_t i = 0; i < c1.size(); ++i) {
            if (c1[i] == p1[i]) c1_has_p1 = true;
            if (c1[i] == p2[i]) c1_has_p2 = true;
        }
        TEST_ASSERT(c1_has_p1, "One-point crossover: child1 contains genes from parent1");
        TEST_ASSERT(c1_has_p2, "One-point crossover: child1 contains genes from parent2");
    }

    // Two-point crossover
    {
        TwoPointCrossover op;
        RealVector p1 = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
        RealVector p2 = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0};
        auto [c1, c2] = op.crossover(p1, p2);

        TEST_ASSERT(c1.size() == p1.size(), "Two-point crossover: child1 size correct");
        TEST_ASSERT(c2.size() == p2.size(), "Two-point crossover: child2 size correct");
    }

    // Uniform crossover
    {
        UniformCrossover op;
        RealVector p1 = {1.0, 1.0, 1.0, 1.0, 1.0};
        RealVector p2 = {2.0, 2.0, 2.0, 2.0, 2.0};
        auto [c1, c2] = op.crossover(p1, p2);

        TEST_ASSERT(c1.size() == p1.size(), "Uniform crossover: child1 size correct");
        for (double gene : c1) {
            TEST_ASSERT(gene == 1.0 || gene == 2.0, "Uniform crossover: gene from parent1 or parent2");
        }
    }

    // Blend crossover (BLX-alpha)
    {
        BlendCrossover op(0.5);
        RealVector p1 = {0.0, 0.0, 0.0};
        RealVector p2 = {1.0, 1.0, 1.0};
        auto [c1, c2] = op.crossover(p1, p2);

        TEST_ASSERT(c1.size() == p1.size(), "Blend crossover: child1 size correct");
        // BLX-alpha can exceed bounds slightly, check values are in reasonable range
        for (double gene : c1) {
            TEST_ASSERT(gene >= -0.5 && gene <= 1.5, "Blend crossover: gene in expected range");
        }
    }

    // Simulated Binary crossover
    {
        SimulatedBinaryCrossover op(2.0);
        RealVector p1 = {1.0, 2.0, 3.0};
        RealVector p2 = {4.0, 5.0, 6.0};
        auto [c1, c2] = op.crossover(p1, p2);

        TEST_ASSERT(c1.size() == p1.size(), "SBX crossover: child1 size correct");
    }

    // Binary crossover
    {
        OnePointCrossover op;
        BitString b1 = {true, true, false, false};
        BitString b2 = {false, false, true, true};
        auto [c1, c2] = op.crossover(b1, b2);

        TEST_ASSERT(c1.size() == b1.size(), "Binary crossover: child1 size correct");
        TEST_ASSERT(c2.size() == b2.size(), "Binary crossover: child2 size correct");
    }
}

// =============================================================================
// Test mutation operators
// =============================================================================
void testMutationOperators() {
    std::cout << "\n=== Testing Mutation Operators ===" << std::endl;

    MutationOperators mutOps;

    // Gaussian mutation
    {
        RealVector individual = {0.5, 0.5, 0.5, 0.5, 0.5};
        RealVector lower = {0.0, 0.0, 0.0, 0.0, 0.0};
        RealVector upper = {1.0, 1.0, 1.0, 1.0, 1.0};
        RealVector copy = individual;

        mutOps.gaussianMutation(copy, 1.0, 0.1, lower, upper);  // pm=1.0 ensures mutation

        TEST_ASSERT(copy.size() == individual.size(), "Gaussian mutation: size preserved");
        bool any_changed = false;
        for (size_t i = 0; i < copy.size(); ++i) {
            if (copy[i] != individual[i]) any_changed = true;
        }
        TEST_ASSERT(any_changed, "Gaussian mutation with pm=1.0: at least one gene changed");

        // Values should be clamped to bounds
        for (size_t i = 0; i < copy.size(); ++i) {
            TEST_ASSERT(copy[i] >= lower[i] && copy[i] <= upper[i],
                       "Gaussian mutation: values within bounds");
        }
    }

    // Uniform mutation
    {
        RealVector individual = {0.5, 0.5, 0.5};
        RealVector lower = {0.0, 0.0, 0.0};
        RealVector upper = {1.0, 1.0, 1.0};
        RealVector copy = individual;

        mutOps.uniformMutation(copy, 1.0, lower, upper);  // pm=1.0

        TEST_ASSERT(copy.size() == individual.size(), "Uniform mutation: size preserved");
        for (size_t i = 0; i < copy.size(); ++i) {
            TEST_ASSERT(copy[i] >= lower[i] && copy[i] <= upper[i],
                       "Uniform mutation: values within bounds");
        }
    }

    // Bit-flip mutation
    {
        std::vector<bool> chromosome = {true, false, true, false, true};
        std::vector<bool> copy = chromosome;

        mutOps.bitFlipMutation(copy, 1.0);  // pm=1.0 flips all bits

        TEST_ASSERT(copy.size() == chromosome.size(), "Bit-flip mutation: size preserved");
        for (size_t i = 0; i < copy.size(); ++i) {
            TEST_ASSERT(copy[i] == !chromosome[i], "Bit-flip with pm=1.0: all bits flipped");
        }
    }

    // No mutation (pm=0.0)
    {
        RealVector individual = {1.0, 2.0, 3.0};
        RealVector lower = {0.0, 0.0, 0.0};
        RealVector upper = {5.0, 5.0, 5.0};
        RealVector copy = individual;

        mutOps.gaussianMutation(copy, 0.0, 0.1, lower, upper);  // pm=0.0 no mutation

        TEST_ASSERT(copy == individual, "Gaussian mutation with pm=0.0: no change");
    }

    // Invalid probability should throw
    {
        RealVector individual = {0.5};
        RealVector lower = {0.0};
        RealVector upper = {1.0};
        bool threw = false;
        try {
            mutOps.gaussianMutation(individual, -0.1, 0.1, lower, upper);
        } catch (...) {
            threw = true;
        }
        TEST_ASSERT(threw, "Gaussian mutation with invalid pm: throws exception");
    }
}

// =============================================================================
// Test selection operators
// =============================================================================
void testSelectionOperators() {
    std::cout << "\n=== Testing Selection Operators ===" << std::endl;

    // Create a population with known fitness values
    std::vector<Individual> population;
    for (int i = 0; i < 10; ++i) {
        Individual ind;
        ind.fitness = static_cast<double>(i + 1);  // fitness 1..10
        ind.genes = {static_cast<double>(i)};
        population.push_back(ind);
    }

    // Tournament selection
    {
        TournamentSelection selector(3);
        auto selected = selector.select(population, 5);
        TEST_ASSERT(selected.size() == 5, "Tournament selection: returns correct count");
        // All selected should have valid fitness
        for (const auto& ind : selected) {
            TEST_ASSERT(ind.fitness >= 1.0 && ind.fitness <= 10.0,
                       "Tournament selection: fitness in valid range");
        }
    }

    // With large tournament size, should prefer higher fitness
    {
        TournamentSelection selector(population.size());  // whole population
        auto selected = selector.select(population, 10);
        TEST_ASSERT(selected.size() == 10, "Tournament (full pop): returns correct count");
        // With full-population tournament, should always pick the best
        bool has_best = false;
        for (const auto& ind : selected) {
            if (ind.fitness == 10.0) has_best = true;
        }
        TEST_ASSERT(has_best, "Tournament (full pop): best individual is selected");
    }

    // Roulette wheel selection
    {
        RouletteWheelSelection selector;
        auto selected = selector.select(population, 5);
        TEST_ASSERT(selected.size() == 5, "Roulette selection: returns correct count");
        for (const auto& ind : selected) {
            TEST_ASSERT(ind.fitness >= 1.0 && ind.fitness <= 10.0,
                       "Roulette selection: fitness in valid range");
        }
    }

    // Rank-based selection
    {
        RankBasedSelection selector;
        auto selected = selector.select(population, 5);
        TEST_ASSERT(selected.size() == 5, "Rank-based selection: returns correct count");
    }
}

// =============================================================================
// Integration test: run a complete GA optimization
// =============================================================================
void testCompleteGAOptimization() {
    std::cout << "\n=== Integration Test: Complete GA Optimization ===" << std::endl;

    // Use Rastrigin function, optimize for 20 generations
    const int POP_SIZE = 30;
    const int GENERATIONS = 20;
    const int CHROM_LEN = 5;
    const double LOWER = -5.12;
    const double UPPER = 5.12;

    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(LOWER, UPPER);

    // Initialize population
    std::vector<Individual> population;
    for (int i = 0; i < POP_SIZE; ++i) {
        Individual ind;
        for (int j = 0; j < CHROM_LEN; ++j) {
            ind.genes.push_back(dist(rng));
        }
        ind.fitness = rastriginFitness(ind.genes);
        population.push_back(ind);
    }

    // Sort by fitness (descending)
    std::sort(population.begin(), population.end(),
              [](const Individual& a, const Individual& b) {
                  return a.fitness > b.fitness;
              });

    double initial_best = population[0].fitness;

    OnePointCrossover crossover_op;
    MutationOperators mutation_op;
    TournamentSelection selection_op(3);

    std::vector<double> lower_bounds(CHROM_LEN, LOWER);
    std::vector<double> upper_bounds(CHROM_LEN, UPPER);

    for (int gen = 0; gen < GENERATIONS; ++gen) {
        auto parents = selection_op.select(population, POP_SIZE);
        std::vector<Individual> new_pop;

        // Elite preservation
        new_pop.push_back(population[0]);

        // Crossover and mutation
        while ((int)new_pop.size() < POP_SIZE) {
            std::uniform_int_distribution<int> pidx(0, (int)parents.size() - 1);
            const auto& p1 = parents[pidx(rng)];
            const auto& p2 = parents[pidx(rng)];

            auto [g1, g2] = crossover_op.crossover(p1.genes, p2.genes);

            mutation_op.gaussianMutation(g1, 0.1, 0.5, lower_bounds, upper_bounds);
            mutation_op.gaussianMutation(g2, 0.1, 0.5, lower_bounds, upper_bounds);

            Individual c1, c2;
            c1.genes = g1; c1.fitness = rastriginFitness(g1);
            c2.genes = g2; c2.fitness = rastriginFitness(g2);

            new_pop.push_back(c1);
            if ((int)new_pop.size() < POP_SIZE) new_pop.push_back(c2);
        }

        population = new_pop;
        std::sort(population.begin(), population.end(),
                  [](const Individual& a, const Individual& b) {
                      return a.fitness > b.fitness;
                  });
    }

    double final_best = population[0].fitness;

    TEST_ASSERT(final_best > 0.0, "Integration test: final best fitness > 0");
    TEST_ASSERT(initial_best >= 0.0 && final_best >= 0.0,
               "Integration test: fitness values are non-negative");
    // Fitness should generally improve (or stay the same due to elitism)
    TEST_ASSERT(final_best >= initial_best * 0.5,
               "Integration test: optimization did not drastically worsen");

    std::cout << "  Initial best fitness: " << initial_best << std::endl;
    std::cout << "  Final best fitness:   " << final_best << std::endl;
    std::cout << "  Best chromosome: ";
    for (double g : population[0].genes) std::cout << g << " ";
    std::cout << std::endl;
}

// =============================================================================
// Main
// =============================================================================
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << " Genetic Algorithm Module Unit Tests" << std::endl;
    std::cout << "============================================" << std::endl;

    try {
        testFitnessFunctions();
        testCrossoverOperators();
        testMutationOperators();
        testSelectionOperators();
        testCompleteGAOptimization();
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        tests_failed++;
    }

    std::cout << "\n============================================" << std::endl;
    std::cout << " Results: " << tests_passed << " passed, "
              << tests_failed << " failed" << std::endl;
    std::cout << "============================================" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
