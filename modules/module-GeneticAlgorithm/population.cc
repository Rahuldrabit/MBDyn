#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <random>
#include <ctime>

class HFElemPopulation {
private:
    double m_dAmplitudeLow;  // Lower bound
    double m_dAmplitudeUpp;  // Upper bound 
    std::vector<double> m_population;
    
public:
    HFElemPopulation(double low = 0.1, double upp = 10.0) 
        : m_dAmplitudeLow(low), m_dAmplitudeUpp(upp) {}

    // Generate initial random population of amplitudes
    void generatePopulation(size_t popSize) {
        std::mt19937 gen(std::time(0));
        std::uniform_real_distribution<> dis(m_dAmplitudeLow, m_dAmplitudeUpp);
        
        m_population.resize(popSize);
        for(size_t i = 0; i < popSize; i++) {
            m_population[i] = dis(gen);
        }
    }

    // Save population to CSV file
    bool saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if(!file.is_open()) {
            return false;
        }

        file << "amplitude\n";
        for(const auto& amp : m_population) {
            file << amp << "\n";
        }
        return true;
    }

    // Get population
    const std::vector<double>& getPopulation() const {
        return m_population;
    }
};