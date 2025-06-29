#ifndef SELECTION_OPERATOR_H
#define SELECTION_OPERATOR_H

#include <vector>

// Individual structure for GA population
struct Individual {
    std::vector<double> genes;
    double fitness;
};

// Tournament selection operator
std::vector<unsigned int> TournamentSelection(std::vector<Individual>& Population, unsigned int TournamentSize);

// Roulette wheel selection operator
std::vector<unsigned int> RouletteWheelSelection(std::vector<Individual>& Population, unsigned int NumSelections = 1);

// Rank selection operator
std::vector<unsigned int> RankSelection(std::vector<Individual>& Population, unsigned int NumSelections = 1);

// Stochastic universal sampling selection operator
std::vector<unsigned int> StochasticUniversalSampling(std::vector<Individual>& Population, unsigned int NumSelections);

// Elitism selection operator
std::vector<unsigned int> ElitismSelection(std::vector<Individual>& Population, unsigned int NumElites);

#endif
