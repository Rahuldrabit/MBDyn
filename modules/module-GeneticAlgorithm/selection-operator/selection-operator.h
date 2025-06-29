#ifndef SELECTION_OPERATOR_H
#define SELECTION_OPERATOR_H

#include <vector>

// Individual structure for GA population
struct Individual {
    std::vector<double> genes;
    double fitness;
    int age = 0;  // For age-based replacement
    
    // Constructor
    Individual() = default;
    Individual(const std::vector<double>& g, double f, int a = 0) : genes(g), fitness(f), age(a) {}
};

// Multiobjective individual structure
struct MultiObjectiveIndividual {
    std::vector<double> genes;
    std::vector<double> objectives;
    int dominationCount = 0;
    std::vector<int> dominatedSolutions;
    int rank = 0;
    double crowdingDistance = 0.0;
    int age = 0;
    
    MultiObjectiveIndividual() = default;
    MultiObjectiveIndividual(const std::vector<double>& g, const std::vector<double>& obj) 
        : genes(g), objectives(obj) {}
};

// Replacement strategies enum
enum class ReplacementStrategy {
    GENERATIONAL,
    STEADY_STATE,
    MU_LAMBDA,
    MU_PLUS_LAMBDA,
    ELITISM,
    REPLACE_WORST,
    AGE_BASED,
    CROWDING,
    DELETE_OLDEST,
    DELETE_RANDOM,
    DELETE_WORST
};

// ======================== PARENT SELECTION OPERATORS ========================

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

// ======================== SURVIVOR SELECTION OPERATORS ========================

// Generational replacement - replace entire population with offspring
std::vector<Individual> GenerationalReplacement(const std::vector<Individual>& offspring);

// Steady-state selection - replace few worst individuals
std::vector<Individual> SteadyStateSelection(std::vector<Individual>& population, 
                                           const std::vector<Individual>& offspring,
                                           unsigned int numReplace);

// (μ, λ) selection - select μ best from λ offspring only
std::vector<Individual> MuLambdaSelection(const std::vector<Individual>& offspring, 
                                         unsigned int mu);

// (μ + λ) selection - select μ best from combined parents and offspring
std::vector<Individual> MuPlusLambdaSelection(const std::vector<Individual>& parents,
                                             const std::vector<Individual>& offspring,
                                             unsigned int mu);

// Elitist replacement with specified number of elites
std::vector<Individual> ElitistReplacement(const std::vector<Individual>& parents,
                                          const std::vector<Individual>& offspring,
                                          unsigned int numElites);

// Replace worst individuals (GENITOR style)
std::vector<Individual> ReplaceWorst(std::vector<Individual>& population,
                                    const std::vector<Individual>& offspring);

// Age-based replacement
std::vector<Individual> AgeBasedReplacement(std::vector<Individual>& population,
                                           const std::vector<Individual>& offspring);

// Round-robin tournament for survivors
std::vector<Individual> RoundRobinTournament(const std::vector<Individual>& combined,
                                            unsigned int mu, unsigned int numCompetitions);

// Crowding-based replacement
std::vector<Individual> CrowdingReplacement(std::vector<Individual>& population,
                                           const std::vector<Individual>& offspring,
                                           unsigned int crowdingFactor);

// Deterministic crowding
std::vector<Individual> DeterministicCrowding(std::vector<Individual>& parents,
                                             const std::vector<Individual>& offspring);

// Restricted tournament selection
std::vector<Individual> RestrictedTournamentSelection(std::vector<Individual>& population,
                                                     const std::vector<Individual>& offspring,
                                                     unsigned int crowdingFactor);

// Delete-based strategies
std::vector<Individual> DeleteOldest(std::vector<Individual>& population,
                                    const std::vector<Individual>& offspring);

std::vector<Individual> DeleteRandom(std::vector<Individual>& population,
                                     const std::vector<Individual>& offspring);

std::vector<Individual> DeleteWorstSurvival(std::vector<Individual>& population,
                                           const std::vector<Individual>& offspring);

// Conservative selection
std::vector<Individual> ConservativeSelection(std::vector<Individual>& population,
                                             const std::vector<Individual>& offspring);

// Differential Evolution style replacement
std::vector<Individual> DifferentialEvolutionReplacement(std::vector<Individual>& population,
                                                        const std::vector<Individual>& offspring);

// ======================== MULTIOBJECTIVE SURVIVOR SELECTION ========================

// NSGA-II survivor selection
std::vector<MultiObjectiveIndividual> NSGAIISurvivorSelection(
    const std::vector<MultiObjectiveIndividual>& combined, unsigned int populationSize);

// SPEA2 survivor selection
std::vector<MultiObjectiveIndividual> SPEA2SurvivorSelection(
    const std::vector<MultiObjectiveIndividual>& combined, unsigned int archiveSize);

// ======================== UTILITY FUNCTIONS ========================

// Calculate Euclidean distance between two individuals
double calculateDistance(const Individual& ind1, const Individual& ind2);

// Check if individual1 dominates individual2 (for multiobjective)
bool dominates(const MultiObjectiveIndividual& ind1, const MultiObjectiveIndividual& ind2);

// Calculate crowding distance for NSGA-II
void calculateCrowdingDistance(std::vector<MultiObjectiveIndividual>& front);

// Fast non-dominated sorting for NSGA-II
std::vector<std::vector<int>> fastNonDominatedSort(std::vector<MultiObjectiveIndividual>& population);

#endif
