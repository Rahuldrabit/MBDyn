#pragma once
#include <vector>
#include <cmath>

class HFElemFitness {
private:
    double m_targetRMS;     // Target RMS value
    double m_tolerance;      // Convergence tolerance
    
public:
    HFElemFitness(double targetRMS = 1.0, double tol = 1e-6)
        : m_targetRMS(targetRMS), m_tolerance(tol) {}

    // Calculate fitness for a single amplitude value
    double calculateFitness(double amplitude, double actualRMS) const {
        // Fitness based on how close amplitude gets to target RMS
        // Higher fitness = better
        double error = std::abs(actualRMS - m_targetRMS);
        
        if(error < m_tolerance) {
            return 1.0; // Perfect match
        }
        
        // Inverse of error - larger error = smaller fitness
        return 1.0 / (1.0 + error);
    }

    // Calculate fitness for entire population
    std::vector<double> evaluatePopulation(
        const std::vector<double>& amplitudes,
        const std::vector<double>& rmsValues) const {
        
        std::vector<double> fitness;
        fitness.reserve(amplitudes.size());
        
        for(size_t i = 0; i < amplitudes.size(); i++) {
            fitness.push_back(calculateFitness(amplitudes[i], rmsValues[i]));
        }
        return fitness;
    }
};