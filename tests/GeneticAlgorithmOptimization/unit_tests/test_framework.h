#ifndef GA_TEST_FRAMEWORK_H
#define GA_TEST_FRAMEWORK_H

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <functional>
#include <random>
#include <chrono>

// Test framework macros
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << #condition \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_NEAR(actual, expected, tolerance) \
    do { \
        if (std::abs((actual) - (expected)) > (tolerance)) { \
            std::cerr << "ASSERTION FAILED: " << #actual << " (" << (actual) \
                      << ") not near " << #expected << " (" << (expected) \
                      << ") within " << (tolerance) \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "Running " << #test_func << "... "; \
        if (test_func()) { \
            std::cout << "PASSED" << std::endl; \
            tests_passed++; \
        } else { \
            std::cout << "FAILED" << std::endl; \
            tests_failed++; \
        } \
        tests_total++; \
    } while(0)

// Base classes for GA testing
class Individual {
public:
    std::vector<double> genes;
    double fitness;
    bool evaluated;
    
    Individual(size_t size) : genes(size, 0.0), fitness(0.0), evaluated(false) {}
    
    Individual(const std::vector<double>& g) : genes(g), fitness(0.0), evaluated(false) {}
    
    void randomize(double min_val = 0.0, double max_val = 1.0) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(min_val, max_val);
        
        for (auto& gene : genes) {
            gene = dis(gen);
        }
        evaluated = false;
    }
};

class Population {
public:
    std::vector<Individual> individuals;
    
    Population(size_t pop_size, size_t gene_size) {
        individuals.reserve(pop_size);
        for (size_t i = 0; i < pop_size; ++i) {
            individuals.emplace_back(gene_size);
            individuals.back().randomize();
        }
    }
    
    void evaluate(std::function<double(const Individual&)> fitness_func) {
        for (auto& ind : individuals) {
            if (!ind.evaluated) {
                ind.fitness = fitness_func(ind);
                ind.evaluated = true;
            }
        }
    }
    
    void sort_by_fitness() {
        std::sort(individuals.begin(), individuals.end(),
                  [](const Individual& a, const Individual& b) {
                      return a.fitness > b.fitness;  // Descending order (maximization)
                  });
    }
    
    Individual& best() {
        sort_by_fitness();
        return individuals[0];
    }
    
    Individual& worst() {
        sort_by_fitness();
        return individuals.back();
    }
    
    double average_fitness() const {
        double sum = 0.0;
        for (const auto& ind : individuals) {
            sum += ind.fitness;
        }
        return sum / individuals.size();
    }
};

// Test fitness functions
namespace TestFunctions {
    // Simple sphere function (unimodal)
    double sphere(const Individual& ind) {
        double sum = 0.0;
        for (double gene : ind.genes) {
            sum += gene * gene;
        }
        return -sum;  // Negative for maximization
    }
    
    // Rastrigin function (multimodal)
    double rastrigin(const Individual& ind) {
        double sum = 0.0;
        const double A = 10.0;
        const size_t n = ind.genes.size();
        
        for (double gene : ind.genes) {
            sum += gene * gene - A * std::cos(2 * M_PI * gene);
        }
        return -(A * n + sum);  // Negative for maximization
    }
    
    // Rosenbrock function (valley-shaped)
    double rosenbrock(const Individual& ind) {
        double sum = 0.0;
        for (size_t i = 0; i < ind.genes.size() - 1; ++i) {
            double x = ind.genes[i];
            double y = ind.genes[i + 1];
            sum += 100.0 * (y - x*x) * (y - x*x) + (1 - x) * (1 - x);
        }
        return -sum;  // Negative for maximization
    }
    
    // Ackley function (highly multimodal)
    double ackley(const Individual& ind) {
        double sum1 = 0.0, sum2 = 0.0;
        const size_t n = ind.genes.size();
        
        for (double gene : ind.genes) {
            sum1 += gene * gene;
            sum2 += std::cos(2 * M_PI * gene);
        }
        
        double result = -20.0 * std::exp(-0.2 * std::sqrt(sum1 / n)) 
                       - std::exp(sum2 / n) + 20.0 + M_E;
        return -result;  // Negative for maximization
    }
}

// Statistical testing utilities
class StatisticalTest {
public:
    static bool chi_square_test(const std::vector<int>& observed, 
                               const std::vector<double>& expected,
                               double significance_level = 0.05) {
        if (observed.size() != expected.size()) {
            return false;
        }
        
        double chi_square = 0.0;
        for (size_t i = 0; i < observed.size(); ++i) {
            if (expected[i] > 0) {
                double diff = observed[i] - expected[i];
                chi_square += (diff * diff) / expected[i];
            }
        }
        
        // Critical value for 95% confidence (degrees of freedom = n-1)
        // This is a simplified version; for precise testing, use proper chi-square tables
        int df = observed.size() - 1;
        double critical_value = 3.841;  // For df=1, alpha=0.05
        if (df > 1) critical_value = 5.991;  // For df=2, alpha=0.05
        if (df > 2) critical_value = 7.815;  // For df=3, alpha=0.05
        
        return chi_square < critical_value;
    }
    
    static double mean(const std::vector<double>& data) {
        double sum = 0.0;
        for (double val : data) {
            sum += val;
        }
        return sum / data.size();
    }
    
    static double variance(const std::vector<double>& data) {
        double m = mean(data);
        double sum_sq = 0.0;
        for (double val : data) {
            double diff = val - m;
            sum_sq += diff * diff;
        }
        return sum_sq / (data.size() - 1);
    }
    
    static double standard_deviation(const std::vector<double>& data) {
        return std::sqrt(variance(data));
    }
};

// Performance measurement utilities
class PerformanceMeter {
public:
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    
    static TimePoint start_timer() {
        return std::chrono::high_resolution_clock::now();
    }
    
    static double end_timer(TimePoint start) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0;  // Return milliseconds
    }
    
    static void benchmark_function(std::function<void()> func, 
                                  const std::string& name,
                                  int iterations = 1000) {
        auto start = start_timer();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        double elapsed = end_timer(start);
        
        std::cout << "Benchmark " << name << ": " 
                  << elapsed / iterations << " ms per iteration" << std::endl;
    }
};

#endif // GA_TEST_FRAMEWORK_H
