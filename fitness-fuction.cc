#include <iostream>
#include <cmath>
#include <algorithm>
#include "fitness-function.h"

// generate random fitness value for testing between 0 and 100 based on a uniform distribution
double generateRandomFitness() {
    return static_cast<double>(rand()) / RAND_MAX * 100.0;
}

// Rastrigin function implementation
double rastriginFunction(const std::vector<double>& x) {
    const double A = 10.0;
    const double PI = 3.14159265358979323846;
    double sum = A * x.size();
    
    for (double xi : x) {
        sum += xi * xi - A * std::cos(2.0 * PI * xi);
    }
    
    return sum;
}

// Ackley function implementation
double ackleyFunction(const std::vector<double>& x) {
    const double a = 20.0;
    const double b = 0.2;
    const double c = 2.0 * 3.14159265358979323846;
    
    double sum1 = 0.0;
    double sum2 = 0.0;
    
    for (double xi : x) {
        sum1 += xi * xi;
        sum2 += std::cos(c * xi);
    }
    
    double n = static_cast<double>(x.size());
    double term1 = -a * std::exp(-b * std::sqrt(sum1 / n));
    double term2 = -std::exp(sum2 / n);
    
    return term1 + term2 + a + std::exp(1.0);
}

// Schwefel function implementation
double schwefelFunction(const std::vector<double>& x) {
    double sum = 0.0;
    
    for (double xi : x) {
        sum += xi * std::sin(std::sqrt(std::abs(xi)));
    }
    
    return 418.9829 * x.size() - sum;
}

// Fitness wrapper functions (convert minimization to maximization)
// Using transformation: fitness = 1000 / (1 + function_value)
double rastriginFitness(const std::vector<double>& x) {
    double value = rastriginFunction(x);
    return 1000.0 / (1.0 + value);
}

double ackleyFitness(const std::vector<double>& x) {
    double value = ackleyFunction(x);
    return 1000.0 / (1.0 + value);
}

double schwefelFitness(const std::vector<double>& x) {
    double value = schwefelFunction(x);
    return 1000.0 / (1.0 + value);
}
