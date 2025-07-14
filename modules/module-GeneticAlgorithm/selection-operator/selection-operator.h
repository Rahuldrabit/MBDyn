#ifndef SELECTION_OPERATOR_H
#define SELECTION_OPERATOR_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
struct Individual {
    double fitness;
    std::vector<double> genes;
    int age = 0;
    
    Individual() : fitness(0.0) {}
    Individual(double f) : fitness(f) {}
    Individual(const std::vector<double>& g, double f) : fitness(f), genes(g) {}
    
    bool operator<(const Individual& other) const {
        return fitness < other.fitness;
    }
};

struct MultiObjectiveIndividual {
    std::vector<double> objectives;
    std::vector<double> genes;
    double crowdingDistance = 0.0;
    int rank = 0;
    int dominationCount = 0;
    std::vector<int> dominatedSolutions;
    int age = 0;
};

// Selection operator base class
class SelectionOperator {
protected:
    std::string name;
    
public:
    SelectionOperator(const std::string& op_name) : name(op_name) {}
    virtual ~SelectionOperator() = default;
    
    virtual std::vector<Individual> select(const std::vector<Individual>& population, 
                                         size_t count) = 0;
    virtual std::string getName() const { return name; }
};

// Tournament selection
class TournamentSelection : public SelectionOperator {
private:
    size_t tournament_size;
    
public:
    TournamentSelection(size_t size = 3) 
        : SelectionOperator("Tournament"), tournament_size(size) {}
    
    std::vector<Individual> select(const std::vector<Individual>& population, 
                                 size_t count) override;
};

// Roulette wheel selection
class RouletteWheelSelection : public SelectionOperator {
public:
    RouletteWheelSelection() : SelectionOperator("RouletteWheel") {}
    
    std::vector<Individual> select(const std::vector<Individual>& population, 
                                 size_t count) override;
};

// Rank selection
class RankSelection : public SelectionOperator {
private:
    double selection_pressure;
    
public:
    RankSelection(double pressure = 2.0) 
        : SelectionOperator("Rank"), selection_pressure(pressure) {}
    
    std::vector<Individual> select(const std::vector<Individual>& population, 
                                 size_t count) override;
};

// Selection types
using BitString = std::vector<bool>;
using RealVector = std::vector<double>;
using IntVector = std::vector<int>;
using Permutation = std::vector<int>;

// Function declarations for standalone selection operators
std::vector<unsigned int> TournamentSelection(std::vector<Individual>& Population, unsigned int TournamentSize);
std::vector<unsigned int> RouletteWheelSelection(std::vector<Individual>& Population, unsigned int NumSelections);
std::vector<unsigned int> RankSelection(std::vector<Individual>& Population, unsigned int NumSelections);
std::vector<unsigned int> StochasticUniversalSampling(std::vector<Individual>& Population, unsigned int NumSelections);
std::vector<unsigned int> ElitismSelection(std::vector<Individual>& Population, unsigned int NumElites);

// Utility functions
double calculateDistance(const Individual& ind1, const Individual& ind2);
bool dominates(const MultiObjectiveIndividual& ind1, const MultiObjectiveIndividual& ind2);
void calculateCrowdingDistance(std::vector<MultiObjectiveIndividual>& front);
std::vector<std::vector<int>> fastNonDominatedSort(std::vector<MultiObjectiveIndividual>& population);

#endif // SELECTION_OPERATOR_H
