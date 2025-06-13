#include "test_framework.h"
#include <algorithm>
#include <map>

// Mock selection operators for testing
class SelectionOperators {
public:
    // Tournament selection
    static Individual tournament_selection(const Population& pop, int tournament_size) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, pop.individuals.size() - 1);
        
        Individual best = pop.individuals[dis(gen)];
        for (int i = 1; i < tournament_size; ++i) {
            Individual candidate = pop.individuals[dis(gen)];
            if (candidate.fitness > best.fitness) {
                best = candidate;
            }
        }
        return best;
    }
    
    // Roulette wheel selection
    static Individual roulette_wheel_selection(const Population& pop) {
        // Calculate total fitness (handle negative values)
        double min_fitness = std::min_element(pop.individuals.begin(), pop.individuals.end(),
            [](const Individual& a, const Individual& b) { return a.fitness < b.fitness; })->fitness;
        
        double offset = min_fitness < 0 ? -min_fitness + 1.0 : 0.0;
        double total_fitness = 0.0;
        for (const auto& ind : pop.individuals) {
            total_fitness += ind.fitness + offset;
        }
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, total_fitness);
        double selection_point = dis(gen);
        
        double cumulative = 0.0;
        for (const auto& ind : pop.individuals) {
            cumulative += ind.fitness + offset;
            if (cumulative >= selection_point) {
                return ind;
            }
        }
        return pop.individuals.back();  // Fallback
    }
    
    // Rank-based selection
    static Individual rank_based_selection(const Population& pop) {
        std::vector<std::pair<double, size_t>> ranked;
        for (size_t i = 0; i < pop.individuals.size(); ++i) {
            ranked.emplace_back(pop.individuals[i].fitness, i);
        }
        
        std::sort(ranked.begin(), ranked.end(), std::greater<>());
        
        // Linear ranking: rank 1 gets weight n, rank 2 gets weight n-1, etc.
        double total_weight = 0.0;
        for (size_t i = 0; i < ranked.size(); ++i) {
            total_weight += (ranked.size() - i);
        }
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, total_weight);
        double selection_point = dis(gen);
        
        double cumulative = 0.0;
        for (size_t i = 0; i < ranked.size(); ++i) {
            cumulative += (ranked.size() - i);
            if (cumulative >= selection_point) {
                return pop.individuals[ranked[i].second];
            }
        }
        return pop.individuals[ranked[0].second];  // Fallback to best
    }
};

// Test functions
bool test_tournament_selection_pressure() {
    Population pop(100, 5);
    pop.evaluate(TestFunctions::sphere);
    pop.sort_by_fitness();
    
    // Test different tournament sizes
    std::vector<int> tournament_sizes = {2, 3, 5, 10};
    
    for (int size : tournament_sizes) {
        std::map<int, int> selection_count;
        int num_selections = 1000;
        
        for (int i = 0; i < num_selections; ++i) {
            Individual selected = SelectionOperators::tournament_selection(pop, size);
            
            // Find index of selected individual
            auto it = std::find_if(pop.individuals.begin(), pop.individuals.end(),
                [&selected](const Individual& ind) {
                    return ind.fitness == selected.fitness && ind.genes == selected.genes;
                });
            
            if (it != pop.individuals.end()) {
                int index = std::distance(pop.individuals.begin(), it);
                selection_count[index]++;
            }
        }
        
        // Better individuals should be selected more often
        int top_10_selections = 0;
        int bottom_10_selections = 0;
        
        for (int i = 0; i < 10; ++i) {
            top_10_selections += selection_count[i];
            bottom_10_selections += selection_count[90 + i];
        }
        
        TEST_ASSERT(top_10_selections > bottom_10_selections);
        
        // Higher tournament size should increase selection pressure
        std::cout << "Tournament size " << size 
                  << ": Top 10% selected " << top_10_selections 
                  << " times, Bottom 10% selected " << bottom_10_selections << " times" << std::endl;
    }
    
    return true;
}

bool test_roulette_wheel_selection() {
    Population pop(50, 3);
    
    // Create population with known fitness values
    for (size_t i = 0; i < pop.individuals.size(); ++i) {
        pop.individuals[i].fitness = i + 1;  // Fitness from 1 to 50
        pop.individuals[i].evaluated = true;
    }
    
    std::map<int, int> selection_count;
    int num_selections = 5000;
    
    for (int i = 0; i < num_selections; ++i) {
        Individual selected = SelectionOperators::roulette_wheel_selection(pop);
        
        // Find index of selected individual
        auto it = std::find_if(pop.individuals.begin(), pop.individuals.end(),
            [&selected](const Individual& ind) {
                return ind.fitness == selected.fitness;
            });
        
        if (it != pop.individuals.end()) {
            int index = std::distance(pop.individuals.begin(), it);
            selection_count[index]++;
        }
    }
    
    // Check that selection probability is proportional to fitness
    double total_fitness = 0.0;
    for (const auto& ind : pop.individuals) {
        total_fitness += ind.fitness;
    }
    
    for (size_t i = 0; i < pop.individuals.size(); ++i) {
        double expected_prob = pop.individuals[i].fitness / total_fitness;
        double actual_prob = static_cast<double>(selection_count[i]) / num_selections;
        
        // Allow 20% deviation from expected probability
        TEST_ASSERT(std::abs(actual_prob - expected_prob) / expected_prob < 0.2);
    }
    
    return true;
}

bool test_rank_based_selection() {
    Population pop(20, 3);
    
    // Create population with known fitness values (some identical)
    for (size_t i = 0; i < pop.individuals.size(); ++i) {
        pop.individuals[i].fitness = (i % 5) + 1;  // Fitness values: 1,2,3,4,5,1,2,3,4,5,...
        pop.individuals[i].evaluated = true;
    }
    
    std::map<double, int> fitness_selection_count;
    int num_selections = 2000;
    
    for (int i = 0; i < num_selections; ++i) {
        Individual selected = SelectionOperators::rank_based_selection(pop);
        fitness_selection_count[selected.fitness]++;
    }
    
    // Higher fitness values should be selected more often
    TEST_ASSERT(fitness_selection_count[5] > fitness_selection_count[4]);
    TEST_ASSERT(fitness_selection_count[4] > fitness_selection_count[3]);
    TEST_ASSERT(fitness_selection_count[3] > fitness_selection_count[2]);
    TEST_ASSERT(fitness_selection_count[2] > fitness_selection_count[1]);
    
    return true;
}

bool test_selection_performance() {
    Population large_pop(1000, 10);
    large_pop.evaluate(TestFunctions::sphere);
    
    // Benchmark different selection methods
    PerformanceMeter::benchmark_function([&]() {
        SelectionOperators::tournament_selection(large_pop, 5);
    }, "Tournament Selection", 1000);
    
    PerformanceMeter::benchmark_function([&]() {
        SelectionOperators::roulette_wheel_selection(large_pop);
    }, "Roulette Wheel Selection", 1000);
    
    PerformanceMeter::benchmark_function([&]() {
        SelectionOperators::rank_based_selection(large_pop);
    }, "Rank-based Selection", 1000);
    
    return true;
}

bool test_selection_edge_cases() {
    // Test with population of size 1
    Population tiny_pop(1, 3);
    tiny_pop.individuals[0].fitness = 10.0;
    tiny_pop.individuals[0].evaluated = true;
    
    Individual selected = SelectionOperators::tournament_selection(tiny_pop, 1);
    TEST_ASSERT(selected.fitness == 10.0);
    
    // Test with all identical fitness values
    Population uniform_pop(10, 3);
    for (auto& ind : uniform_pop.individuals) {
        ind.fitness = 5.0;
        ind.evaluated = true;
    }
    
    // Should still work without errors
    selected = SelectionOperators::roulette_wheel_selection(uniform_pop);
    TEST_ASSERT(selected.fitness == 5.0);
    
    selected = SelectionOperators::rank_based_selection(uniform_pop);
    TEST_ASSERT(selected.fitness == 5.0);
    
    return true;
}

int main() {
    int tests_passed = 0;
    int tests_failed = 0;
    int tests_total = 0;
    
    std::cout << "Running Selection Operator Tests..." << std::endl;
    
    RUN_TEST(test_tournament_selection_pressure);
    RUN_TEST(test_roulette_wheel_selection);
    RUN_TEST(test_rank_based_selection);
    RUN_TEST(test_selection_performance);
    RUN_TEST(test_selection_edge_cases);
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total: " << tests_total << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;
    std::cout << "Success Rate: " << (100.0 * tests_passed / tests_total) << "%" << std::endl;
    
    return tests_failed == 0 ? 0 : 1;
}
