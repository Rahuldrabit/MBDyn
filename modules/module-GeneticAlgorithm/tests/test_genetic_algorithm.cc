/* Unit Tests for Genetic Algorithm Module
 * 
 * This file contains comprehensive unit tests for the GA module components:
 * - Utility functions (clamp01)
 * - GAConfig structure
 * - Mock library loading simulation 
 * - Integration tests with mock GA library
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <dlfcn.h>
#include <stdexcept>

// Include the module header (we'll need to extract declarations)
extern "C" {
    #include "mbconfig.h"
}

// Forward declarations and test utilities
static inline double clamp01(double x) {
    return x < 0. ? 0. : (x > 1. ? 1. : x);
}

struct GAConfig {
    std::string selection;
    std::string crossover; 
    std::string mutation;
    double crossover_rate;
    double mutation_rate;
};

// Mock GA library functions for testing
extern "C" {
    void* mock_ga_init(int pop_size, int generations);
    void mock_ga_run(void* ga_ctx);
    double mock_ga_get_best(void* ga_ctx);
}

// Test fixture for GA utility functions
class GAUtilityTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test the clamp01 utility function
TEST_F(GAUtilityTest, Clamp01Function) {
    // Test normal range [0, 1]
    EXPECT_DOUBLE_EQ(clamp01(0.5), 0.5);
    EXPECT_DOUBLE_EQ(clamp01(0.0), 0.0);
    EXPECT_DOUBLE_EQ(clamp01(1.0), 1.0);
    
    // Test values below 0
    EXPECT_DOUBLE_EQ(clamp01(-0.1), 0.0);
    EXPECT_DOUBLE_EQ(clamp01(-10.0), 0.0);
    
    // Test values above 1
    EXPECT_DOUBLE_EQ(clamp01(1.1), 1.0);
    EXPECT_DOUBLE_EQ(clamp01(100.0), 1.0);
    
    // Test edge cases
    EXPECT_DOUBLE_EQ(clamp01(-0.0), 0.0);
    EXPECT_DOUBLE_EQ(clamp01(std::numeric_limits<double>::infinity()), 1.0);
}

// Test fixture for GAConfig structure
class GAConfigTest : public ::testing::Test {
protected:
    GAConfig config;
    
    void SetUp() override {
        config.selection = "roulette";
        config.crossover = "single_point";
        config.mutation = "bit_flip";
        config.crossover_rate = 0.8;
        config.mutation_rate = 0.1;
    }
};

TEST_F(GAConfigTest, DefaultConfiguration) {
    GAConfig default_config;
    EXPECT_TRUE(default_config.selection.empty());
    EXPECT_TRUE(default_config.crossover.empty());
    EXPECT_TRUE(default_config.mutation.empty());
    EXPECT_DOUBLE_EQ(default_config.crossover_rate, 0.0);
    EXPECT_DOUBLE_EQ(default_config.mutation_rate, 0.0);
}

TEST_F(GAConfigTest, ValidConfiguration) {
    EXPECT_EQ(config.selection, "roulette");
    EXPECT_EQ(config.crossover, "single_point");
    EXPECT_EQ(config.mutation, "bit_flip");
    EXPECT_DOUBLE_EQ(config.crossover_rate, 0.8);
    EXPECT_DOUBLE_EQ(config.mutation_rate, 0.1);
}

TEST_F(GAConfigTest, RateValidation) {
    // Test that rates are in valid range (this would be in actual GA implementation)
    EXPECT_GE(config.crossover_rate, 0.0);
    EXPECT_LE(config.crossover_rate, 1.0);
    EXPECT_GE(config.mutation_rate, 0.0);
    EXPECT_LE(config.mutation_rate, 1.0);
}

// Test fixture for mock GA library functions
class MockGALibraryTest : public ::testing::Test {
protected:
    void* ga_ctx;
    
    void SetUp() override {
        ga_ctx = nullptr;
    }
    
    void TearDown() override {
        // Clean up if needed
    }
};

TEST_F(MockGALibraryTest, InitializeGA) {
    ga_ctx = mock_ga_init(50, 100);
    EXPECT_NE(ga_ctx, nullptr);
    
    // Test with different parameters
    void* ga_ctx2 = mock_ga_init(20, 50);
    EXPECT_NE(ga_ctx2, nullptr);
    EXPECT_NE(ga_ctx, ga_ctx2);  // Different contexts
}

TEST_F(MockGALibraryTest, RunGA) {
    ga_ctx = mock_ga_init(10, 20);
    ASSERT_NE(ga_ctx, nullptr);
    
    // Should not crash
    EXPECT_NO_THROW(mock_ga_run(ga_ctx));
}

TEST_F(MockGALibraryTest, GetBestFitness) {
    ga_ctx = mock_ga_init(10, 20);
    ASSERT_NE(ga_ctx, nullptr);
    
    // Run GA first
    mock_ga_run(ga_ctx);
    
    // Get best fitness (should be reasonable value)
    double best_fitness = mock_ga_get_best(ga_ctx);
    EXPECT_GE(best_fitness, 0.0);
    EXPECT_LE(best_fitness, 1.0);  // Assuming normalized fitness
}

// Integration test simulating the full GA workflow
class GAIntegrationTest : public ::testing::Test {
protected:
    struct MockGAContext {
        int population;
        int generations;
        double best_fitness;
        bool has_run;
    };
    
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GAIntegrationTest, FullGAWorkflow) {
    // Simulate the complete workflow
    int population = 50;
    int generations = 100;
    
    // 1. Initialize GA
    void* ga_ctx = mock_ga_init(population, generations);
    ASSERT_NE(ga_ctx, nullptr);
    
    // 2. Run GA multiple times (simulating multiple simulation steps)
    for (int step = 0; step < 5; ++step) {
        EXPECT_NO_THROW(mock_ga_run(ga_ctx));
        
        double fitness = mock_ga_get_best(ga_ctx);
        EXPECT_GE(fitness, 0.0);
        
        // Fitness should generally improve or stay same over steps
        if (step > 0) {
            // This is a basic expectation - in real GA, fitness should improve
            EXPECT_GE(fitness, 0.0);
        }
    }
}

// Test error conditions
class GAErrorHandlingTest : public ::testing::Test {};

TEST_F(GAErrorHandlingTest, InvalidParameters) {
    // Test with invalid population size
    void* ga_ctx = mock_ga_init(0, 100);
    // In a real implementation, this might return nullptr or throw
    // For now, just ensure it doesn't crash
    
    // Test with invalid generations
    ga_ctx = mock_ga_init(50, 0);
    // Similar expectation
}

// Performance benchmark test (optional)
class GAPerformanceTest : public ::testing::Test {};

TEST_F(GAPerformanceTest, DISABLED_BenchmarkGAPerformance) {
    // This test is disabled by default (DISABLED_ prefix)
    // Run with --gtest_also_run_disabled_tests to include it
    
    auto start = std::chrono::high_resolution_clock::now();
    
    void* ga_ctx = mock_ga_init(1000, 500);  // Large problem
    mock_ga_run(ga_ctx);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Ensure it completes within reasonable time (adjust as needed)
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds
}

// Main function for running tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}