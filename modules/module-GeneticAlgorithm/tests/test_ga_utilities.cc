/* Additional Unit Tests for GA Utility Functions
 * 
 * This file tests utility functions and helper components
 * that can be extracted from the main module for easier testing.
 */

#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <random>

// Utility functions that could be extracted from GA module
namespace ga_utils {
    
    // Normalize a vector to sum to 1.0 (useful for selection probabilities)
    std::vector<double> normalize_probabilities(const std::vector<double>& values) {
        std::vector<double> result = values;
        double sum = 0.0;
        for (double val : values) {
            sum += val;
        }
        if (sum > 0.0) {
            for (double& val : result) {
                val /= sum;
            }
        }
        return result;
    }
    
    // Select random index based on probabilities (roulette wheel selection)
    int roulette_selection(const std::vector<double>& probabilities, 
                          std::mt19937& rng) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double r = dist(rng);
        double cumulative = 0.0;
        
        for (size_t i = 0; i < probabilities.size(); ++i) {
            cumulative += probabilities[i];
            if (r <= cumulative) {
                return static_cast<int>(i);
            }
        }
        return static_cast<int>(probabilities.size() - 1);
    }
    
    // Tournament selection
    int tournament_selection(const std::vector<double>& fitness, 
                           int tournament_size, std::mt19937& rng) {
        std::uniform_int_distribution<int> dist(0, static_cast<int>(fitness.size() - 1));
        
        int best_idx = dist(rng);
        double best_fitness = fitness[best_idx];
        
        for (int i = 1; i < tournament_size; ++i) {
            int idx = dist(rng);
            if (fitness[idx] > best_fitness) {
                best_idx = idx;
                best_fitness = fitness[idx];
            }
        }
        return best_idx;
    }
    
    // Single-point crossover for binary chromosomes
    std::pair<std::vector<bool>, std::vector<bool>> 
    single_point_crossover(const std::vector<bool>& parent1,
                          const std::vector<bool>& parent2,
                          std::mt19937& rng) {
        std::uniform_int_distribution<int> dist(1, static_cast<int>(parent1.size() - 1));
        int crossover_point = dist(rng);
        
        std::vector<bool> child1 = parent1;
        std::vector<bool> child2 = parent2;
        
        for (int i = crossover_point; i < static_cast<int>(parent1.size()); ++i) {
            child1[i] = parent2[i];
            child2[i] = parent1[i];
        }
        
        return std::make_pair(child1, child2);
    }
    
    // Bit-flip mutation
    std::vector<bool> bit_flip_mutation(const std::vector<bool>& chromosome,
                                       double mutation_rate, std::mt19937& rng) {
        std::vector<bool> result = chromosome;
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        
        for (size_t i = 0; i < result.size(); ++i) {
            if (dist(rng) < mutation_rate) {
                result[i] = !result[i];
            }
        }
        return result;
    }
}

// Test fixture for probability normalization
class ProbabilityTest : public ::testing::Test {
protected:
    std::vector<double> test_values;
    
    void SetUp() override {
        test_values = {1.0, 2.0, 3.0, 4.0};
    }
};

TEST_F(ProbabilityTest, NormalizeProbabilities) {
    auto normalized = ga_utils::normalize_probabilities(test_values);
    
    // Check that sum equals 1.0
    double sum = 0.0;
    for (double val : normalized) {
        sum += val;
    }
    EXPECT_NEAR(sum, 1.0, 1e-10);
    
    // Check relative proportions are maintained
    EXPECT_NEAR(normalized[0], 0.1, 1e-10);  // 1/10
    EXPECT_NEAR(normalized[1], 0.2, 1e-10);  // 2/10
    EXPECT_NEAR(normalized[2], 0.3, 1e-10);  // 3/10
    EXPECT_NEAR(normalized[3], 0.4, 1e-10);  // 4/10
}

TEST_F(ProbabilityTest, NormalizeZeroSum) {
    std::vector<double> zeros = {0.0, 0.0, 0.0};
    auto normalized = ga_utils::normalize_probabilities(zeros);
    
    // Should handle zero sum gracefully
    for (double val : normalized) {
        EXPECT_DOUBLE_EQ(val, 0.0);
    }
}

// Test fixture for selection algorithms
class SelectionTest : public ::testing::Test {
protected:
    std::vector<double> fitness;
    std::mt19937 rng;
    
    void SetUp() override {
        fitness = {0.1, 0.3, 0.2, 0.4};  // Different fitness values
        rng.seed(42);  // Fixed seed for reproducible tests
    }
};

TEST_F(SelectionTest, RouletteSelection) {
    auto probabilities = ga_utils::normalize_probabilities(fitness);
    
    // Test multiple selections
    std::vector<int> selections(1000);
    for (int i = 0; i < 1000; ++i) {
        selections[i] = ga_utils::roulette_selection(probabilities, rng);
    }
    
    // Count frequency of each selection
    std::vector<int> counts(4, 0);
    for (int sel : selections) {
        ASSERT_GE(sel, 0);
        ASSERT_LT(sel, 4);
        counts[sel]++;
    }
    
    // Higher fitness should be selected more often
    EXPECT_GT(counts[3], counts[0]);  // Index 3 has highest fitness
    EXPECT_GT(counts[1], counts[0]);  // Index 1 has higher fitness than 0
}

TEST_F(SelectionTest, TournamentSelection) {
    int tournament_size = 2;
    
    // Test multiple selections
    std::vector<int> selections(1000);
    for (int i = 0; i < 1000; ++i) {
        selections[i] = ga_utils::tournament_selection(fitness, tournament_size, rng);
    }
    
    // Count frequency
    std::vector<int> counts(4, 0);
    for (int sel : selections) {
        ASSERT_GE(sel, 0);
        ASSERT_LT(sel, 4);
        counts[sel]++;
    }
    
    // Higher fitness should be selected more often
    EXPECT_GT(counts[3], counts[0]);
}

// Test fixture for crossover operations
class CrossoverTest : public ::testing::Test {
protected:
    std::vector<bool> parent1, parent2;
    std::mt19937 rng;
    
    void SetUp() override {
        parent1 = {true, true, false, false, true, false};
        parent2 = {false, false, true, true, false, true};
        rng.seed(42);
    }
};

TEST_F(CrossoverTest, SinglePointCrossover) {
    auto children = ga_utils::single_point_crossover(parent1, parent2, rng);
    
    // Children should have same length as parents
    EXPECT_EQ(children.first.size(), parent1.size());
    EXPECT_EQ(children.second.size(), parent2.size());
    
    // Test specific crossover (depends on RNG seed)
    // At minimum, ensure crossover actually occurred
    bool crossover_occurred = false;
    for (size_t i = 0; i < parent1.size(); ++i) {
        if (children.first[i] != parent1[i] || children.second[i] != parent2[i]) {
            crossover_occurred = true;
            break;
        }
    }
    EXPECT_TRUE(crossover_occurred);
}

// Test fixture for mutation operations
class MutationTest : public ::testing::Test {
protected:
    std::vector<bool> chromosome;
    std::mt19937 rng;
    
    void SetUp() override {
        chromosome = {true, false, true, false, true, false, true, false};
        rng.seed(42);
    }
};

TEST_F(MutationTest, BitFlipMutation) {
    double high_mutation_rate = 0.5;  // High rate to ensure some mutations
    auto mutated = ga_utils::bit_flip_mutation(chromosome, high_mutation_rate, rng);
    
    // Should have same length
    EXPECT_EQ(mutated.size(), chromosome.size());
    
    // With high mutation rate, at least some bits should be different
    int differences = 0;
    for (size_t i = 0; i < chromosome.size(); ++i) {
        if (mutated[i] != chromosome[i]) {
            differences++;
        }
    }
    EXPECT_GT(differences, 0);
}

TEST_F(MutationTest, NoMutation) {
    double zero_mutation_rate = 0.0;
    auto mutated = ga_utils::bit_flip_mutation(chromosome, zero_mutation_rate, rng);
    
    // Should be identical to original
    EXPECT_EQ(mutated, chromosome);
}

TEST_F(MutationTest, FullMutation) {
    double full_mutation_rate = 1.0;
    auto mutated = ga_utils::bit_flip_mutation(chromosome, full_mutation_rate, rng);
    
    // All bits should be flipped
    for (size_t i = 0; i < chromosome.size(); ++i) {
        EXPECT_EQ(mutated[i], !chromosome[i]);
    }
}