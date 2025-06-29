#include "selection-operator.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <random>
#include <numeric>
#include <limits>

// ======================== SELECTION OPERATORS ========================

// Tournament selection operator
std::vector<unsigned int> TournamentSelection(std::vector<Individual>& Population, unsigned int TournamentSize) {
    std::vector<unsigned int> SelectedIndices;
    unsigned int PopulationSize = Population.size();

    if (PopulationSize == 0) {
        std::cerr << "Error: Population is empty." << std::endl;
        return {};
    }
    if (TournamentSize == 0) {
        std::cerr << "Error: Invalid tournament size." << std::endl;
        return {};
    }
    
    // Adjust tournament size if necessary
    if (TournamentSize > PopulationSize) {
        std::cerr << "Warning: Tournament size is larger than population size. Using population size." << std::endl;
        TournamentSize = PopulationSize;
    }
    
    // Randomly select individuals for the tournament
    SelectedIndices.reserve(TournamentSize);
    while (SelectedIndices.size() < TournamentSize) {
        unsigned int RandomIndex = rand() % PopulationSize;
        if (std::find(SelectedIndices.begin(), SelectedIndices.end(), RandomIndex) == SelectedIndices.end()) {
            SelectedIndices.push_back(RandomIndex);
        }
    }

    // Find the best individual among the selected ones
    unsigned int BestIndex = SelectedIndices[0];
    for (unsigned int i = 1; i < SelectedIndices.size(); ++i) {
        if (Population[SelectedIndices[i]].fitness > Population[BestIndex].fitness) {
            BestIndex = SelectedIndices[i];
        }
    }

    return {BestIndex};
}

// Roulette wheel selection operator
std::vector<unsigned int> RouletteWheelSelection(std::vector<Individual>& Population, unsigned int NumSelections) {
    std::vector<unsigned int> SelectedIndices;
    unsigned int PopulationSize = Population.size();

    if (PopulationSize == 0) {
        std::cerr << "Error: Population is empty." << std::endl;
        return {};
    }

    // Calculate the total fitness
    double TotalFitness = 0.0;
    for (const auto& individual : Population) {
        TotalFitness += individual.fitness;
    }

    if (TotalFitness <= 0.0) {
        std::cerr << "Error: Total fitness is non-positive." << std::endl;
        return {};
    }

    // Select individuals based on their fitness proportion
    for (unsigned int i = 0; i < NumSelections; ++i) {
        double RandomValue = static_cast<double>(rand()) / RAND_MAX * TotalFitness;
        double CumulativeFitness = 0.0;

        for (unsigned int j = 0; j < PopulationSize; ++j) {
            CumulativeFitness += Population[j].fitness;
            if (CumulativeFitness >= RandomValue) {
                SelectedIndices.push_back(j);
                break;
            }
        }
    }

    return SelectedIndices;
}

// Rank selection operator
std::vector<unsigned int> RankSelection(std::vector<Individual>& Population, unsigned int NumSelections) {
    std::vector<unsigned int> SelectedIndices;
    unsigned int PopulationSize = Population.size();

    if (PopulationSize == 0) {
        std::cerr << "Error: Population is empty." << std::endl;
        return {};
    }

    // Create a vector of indices and sort them based on fitness
    std::vector<unsigned int> Indices(PopulationSize);
    for (unsigned int i = 0; i < PopulationSize; ++i) {
        Indices[i] = i;
    }

    std::sort(Indices.begin(), Indices.end(), [&](unsigned int a, unsigned int b) {
        return Population[a].fitness > Population[b].fitness;
    });

    // Calculate total rank sum for selection probability
    double TotalRankSum = (PopulationSize * (PopulationSize + 1)) / 2.0;

    // Select individuals based on their rank
    for (unsigned int i = 0; i < NumSelections; ++i) {
        double RandomValue = static_cast<double>(rand()) / RAND_MAX * TotalRankSum;
        double CumulativeRank = 0.0;

        for (unsigned int j = 0; j < PopulationSize; ++j) {
            // Higher fitness gets higher rank (PopulationSize for best, 1 for worst)
            CumulativeRank += (PopulationSize - j);
            if (CumulativeRank >= RandomValue) {
                SelectedIndices.push_back(Indices[j]);
                break;
            }
        }
    }

    return SelectedIndices;
}

// Stochastic universal sampling selection operator
std::vector<unsigned int> StochasticUniversalSampling(std::vector<Individual>& Population, unsigned int NumSelections) {
    std::vector<unsigned int> SelectedIndices;
    unsigned int PopulationSize = Population.size();

    if (PopulationSize == 0 || NumSelections == 0) {
        std::cerr << "Error: Population is empty or number of selections is zero." << std::endl;
        return {};
    }

    // Calculate the total fitness
    double TotalFitness = 0.0;
    for (const auto& individual : Population) {
        TotalFitness += individual.fitness;
    }

    if (TotalFitness <= 0.0) {
        std::cerr << "Error: Total fitness is non-positive." << std::endl;
        return {};
    }

    // Calculate the distance between selection points
    double Distance = TotalFitness / NumSelections;
    double StartPoint = static_cast<double>(rand()) / RAND_MAX * Distance;

    // Select individuals using evenly spaced pointers
    double CumulativeFitness = 0.0;
    unsigned int PopIndex = 0;

    for (unsigned int i = 0; i < NumSelections; ++i) {
        double SelectionPoint = StartPoint + i * Distance;

        while (PopIndex < PopulationSize && CumulativeFitness < SelectionPoint) {
            CumulativeFitness += Population[PopIndex].fitness;
            PopIndex++;
        }

        if (PopIndex > 0) {
            SelectedIndices.push_back(PopIndex - 1);
        } else {
            SelectedIndices.push_back(0);
        }
    }

    return SelectedIndices;
}

// Elitism selection operator
std::vector<unsigned int> ElitismSelection(std::vector<Individual>& Population, unsigned int NumElites) {
    std::vector<unsigned int> SelectedIndices;
    unsigned int PopulationSize = Population.size();

    if (PopulationSize == 0 || NumElites == 0) {
        std::cerr << "Error: Population is empty or number of elites is zero." << std::endl;
        return {};
    }

    // Sort individuals based on fitness in descending order
    std::vector<unsigned int> Indices(PopulationSize);
    for (unsigned int i = 0; i < PopulationSize; ++i) {
        Indices[i] = i;
    }

    std::sort(Indices.begin(), Indices.end(), [&](unsigned int a, unsigned int b) {
        return Population[a].fitness > Population[b].fitness;
    });

    // Select the top NumElites individuals
    for (unsigned int i = 0; i < NumElites && i < PopulationSize; ++i) {
        SelectedIndices.push_back(Indices[i]);
    }

    return SelectedIndices;
}

// ======================== SURVIVOR SELECTION OPERATORS ========================

// Generational replacement - replace entire population with offspring
std::vector<Individual> GenerationalReplacement(const std::vector<Individual>& offspring) {
    return offspring;  // Simply return all offspring
}

// Steady-state selection - replace few worst individuals
std::vector<Individual> SteadyStateSelection(std::vector<Individual>& population, 
                                           const std::vector<Individual>& offspring,
                                           unsigned int numReplace) {
    if (numReplace > population.size() || numReplace > offspring.size()) {
        std::cerr << "Error: Invalid number of replacements in steady-state selection." << std::endl;
        return population;
    }
    
    // Sort population by fitness (ascending - worst first)
    std::vector<unsigned int> indices(population.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](unsigned int a, unsigned int b) {
        return population[a].fitness < population[b].fitness;
    });
    
    // Replace worst individuals with offspring
    for (unsigned int i = 0; i < numReplace; ++i) {
        population[indices[i]] = offspring[i];
    }
    
    return population;
}

// (μ, λ) selection - select μ best from λ offspring only
std::vector<Individual> MuLambdaSelection(const std::vector<Individual>& offspring, 
                                         unsigned int mu) {
    if (mu > offspring.size()) {
        std::cerr << "Error: mu greater than offspring size in (μ, λ) selection." << std::endl;
        return offspring;
    }
    
    std::vector<Individual> selected = offspring;
    
    // Sort by fitness (descending - best first)
    std::sort(selected.begin(), selected.end(), [](const Individual& a, const Individual& b) {
        return a.fitness > b.fitness;
    });
    
    selected.resize(mu);
    return selected;
}

// (μ + λ) selection - select μ best from combined parents and offspring
std::vector<Individual> MuPlusLambdaSelection(const std::vector<Individual>& parents,
                                             const std::vector<Individual>& offspring,
                                             unsigned int mu) {
    std::vector<Individual> combined;
    combined.reserve(parents.size() + offspring.size());
    combined.insert(combined.end(), parents.begin(), parents.end());
    combined.insert(combined.end(), offspring.begin(), offspring.end());
    
    if (mu > combined.size()) {
        std::cerr << "Error: mu greater than combined population size in (μ + λ) selection." << std::endl;
        return combined;
    }
    
    // Sort by fitness (descending - best first)
    std::sort(combined.begin(), combined.end(), [](const Individual& a, const Individual& b) {
        return a.fitness > b.fitness;
    });
    
    combined.resize(mu);
    return combined;
}

// Elitist replacement with specified number of elites
std::vector<Individual> ElitistReplacement(const std::vector<Individual>& parents,
                                          const std::vector<Individual>& offspring,
                                          unsigned int numElites) {
    if (numElites > parents.size()) {
        std::cerr << "Warning: Number of elites exceeds parent population size." << std::endl;
        numElites = parents.size();
    }
    
    // Get elite individuals from parents
    std::vector<Individual> sortedParents = parents;
    std::sort(sortedParents.begin(), sortedParents.end(), [](const Individual& a, const Individual& b) {
        return a.fitness > b.fitness;
    });
    
    std::vector<Individual> newPopulation;
    newPopulation.reserve(parents.size());
    
    // Add elites
    for (unsigned int i = 0; i < numElites; ++i) {
        newPopulation.push_back(sortedParents[i]);
    }
    
    // Add offspring to fill remaining slots
    unsigned int remainingSlots = parents.size() - numElites;
    for (unsigned int i = 0; i < remainingSlots && i < offspring.size(); ++i) {
        newPopulation.push_back(offspring[i]);
    }
    
    return newPopulation;
}

// Replace worst individuals (GENITOR style)
std::vector<Individual> ReplaceWorst(std::vector<Individual>& population,
                                    const std::vector<Individual>& offspring) {
    for (const auto& child : offspring) {
        // Find worst individual in population
        auto worstIt = std::min_element(population.begin(), population.end(),
            [](const Individual& a, const Individual& b) {
                return a.fitness < b.fitness;
            });
        
        // Replace if child is better
        if (child.fitness > worstIt->fitness) {
            *worstIt = child;
        }
    }
    
    return population;
}

// Age-based replacement
std::vector<Individual> AgeBasedReplacement(std::vector<Individual>& population,
                                           const std::vector<Individual>& offspring) {
    // Increment age of all individuals
    for (auto& ind : population) {
        ind.age++;
    }
    
    // Replace oldest individuals
    for (const auto& child : offspring) {
        auto oldestIt = std::max_element(population.begin(), population.end(),
            [](const Individual& a, const Individual& b) {
                return a.age < b.age;
            });
        
        Individual newChild = child;
        newChild.age = 0;  // Reset age for new individual
        *oldestIt = newChild;
    }
    
    return population;
}

// Round-robin tournament for survivors
std::vector<Individual> RoundRobinTournament(const std::vector<Individual>& combined,
                                            unsigned int mu, unsigned int numCompetitions) {
    std::vector<int> wins(combined.size(), 0);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, combined.size() - 1);
    
    // Conduct round-robin tournaments
    for (unsigned int i = 0; i < combined.size(); ++i) {
        for (unsigned int comp = 0; comp < numCompetitions; ++comp) {
            int opponent = dis(gen);
            if (opponent != static_cast<int>(i) && combined[i].fitness > combined[opponent].fitness) {
                wins[i]++;
            }
        }
    }
    
    // Select μ individuals with most wins
    std::vector<unsigned int> indices(combined.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](unsigned int a, unsigned int b) {
        return wins[a] > wins[b];
    });
    
    std::vector<Individual> survivors;
    for (unsigned int i = 0; i < mu && i < combined.size(); ++i) {
        survivors.push_back(combined[indices[i]]);
    }
    
    return survivors;
}

// Crowding-based replacement
std::vector<Individual> CrowdingReplacement(std::vector<Individual>& population,
                                           const std::vector<Individual>& offspring,
                                           unsigned int crowdingFactor) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    for (const auto& child : offspring) {
        // Select random subset for crowding
        std::vector<unsigned int> subset;
        std::uniform_int_distribution<> dis(0, population.size() - 1);
        
        for (unsigned int i = 0; i < crowdingFactor && i < population.size(); ++i) {
            unsigned int idx;
            do {
                idx = dis(gen);
            } while (std::find(subset.begin(), subset.end(), idx) != subset.end());
            subset.push_back(idx);
        }
        
        // Find most similar individual in subset
        double minDistance = std::numeric_limits<double>::max();
        unsigned int mostSimilarIdx = subset[0];
        
        for (unsigned int idx : subset) {
            double distance = calculateDistance(child, population[idx]);
            if (distance < minDistance) {
                minDistance = distance;
                mostSimilarIdx = idx;
            }
        }
        
        // Replace most similar individual
        population[mostSimilarIdx] = child;
    }
    
    return population;
}

// Deterministic crowding
std::vector<Individual> DeterministicCrowding(std::vector<Individual>& parents,
                                             const std::vector<Individual>& offspring) {
    if (offspring.size() != parents.size()) {
        std::cerr << "Error: Offspring size must equal parent size for deterministic crowding." << std::endl;
        return parents;
    }
    
    std::vector<Individual> newPopulation = parents;
    
    for (size_t i = 0; i < offspring.size(); i += 2) {
        if (i + 1 < offspring.size() && i + 1 < parents.size()) {
            // Calculate distances between children and parents
            double dist1 = calculateDistance(offspring[i], parents[i]) + 
                          calculateDistance(offspring[i + 1], parents[i + 1]);
            double dist2 = calculateDistance(offspring[i], parents[i + 1]) + 
                          calculateDistance(offspring[i + 1], parents[i]);
            
            // Match children with most similar parents
            if (dist1 <= dist2) {
                if (offspring[i].fitness > parents[i].fitness) {
                    newPopulation[i] = offspring[i];
                }
                if (offspring[i + 1].fitness > parents[i + 1].fitness) {
                    newPopulation[i + 1] = offspring[i + 1];
                }
            } else {
                if (offspring[i].fitness > parents[i + 1].fitness) {
                    newPopulation[i + 1] = offspring[i];
                }
                if (offspring[i + 1].fitness > parents[i].fitness) {
                    newPopulation[i] = offspring[i + 1];
                }
            }
        }
    }
    
    return newPopulation;
}

// Restricted tournament selection
std::vector<Individual> RestrictedTournamentSelection(std::vector<Individual>& population,
                                                     const std::vector<Individual>& offspring,
                                                     unsigned int crowdingFactor) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    for (const auto& child : offspring) {
        // Select random subset for crowding
        std::vector<unsigned int> subset;
        std::uniform_int_distribution<> dis(0, population.size() - 1);
        
        for (unsigned int i = 0; i < crowdingFactor && i < population.size(); ++i) {
            subset.push_back(dis(gen));
        }
        
        // Find most similar individual in subset
        double minDistance = std::numeric_limits<double>::max();
        unsigned int mostSimilarIdx = subset[0];
        
        for (unsigned int idx : subset) {
            double distance = calculateDistance(child, population[idx]);
            if (distance < minDistance) {
                minDistance = distance;
                mostSimilarIdx = idx;
            }
        }
        
        // Replace only if child is fitter
        if (child.fitness > population[mostSimilarIdx].fitness) {
            population[mostSimilarIdx] = child;
        }
    }
    
    return population;
}

// Delete-based strategies
std::vector<Individual> DeleteOldest(std::vector<Individual>& population,
                                    const std::vector<Individual>& offspring) {
    for (const auto& child : offspring) {
        auto oldestIt = std::max_element(population.begin(), population.end(),
            [](const Individual& a, const Individual& b) {
                return a.age < b.age;
            });
        
        Individual newChild = child;
        newChild.age = 0;
        *oldestIt = newChild;
    }
    
    return population;
}

std::vector<Individual> DeleteRandom(std::vector<Individual>& population,
                                     const std::vector<Individual>& offspring) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, population.size() - 1);
    
    for (const auto& child : offspring) {
        unsigned int randomIdx = dis(gen);
        population[randomIdx] = child;
    }
    
    return population;
}

std::vector<Individual> DeleteWorstSurvival(std::vector<Individual>& population,
                                           const std::vector<Individual>& offspring) {
    for (const auto& child : offspring) {
        auto worstIt = std::min_element(population.begin(), population.end(),
            [](const Individual& a, const Individual& b) {
                return a.fitness < b.fitness;
            });
        
        *worstIt = child;
    }
    
    return population;
}

// Conservative selection
std::vector<Individual> ConservativeSelection(std::vector<Individual>& population,
                                             const std::vector<Individual>& offspring) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, population.size() - 1);
    
    // Find best individual
    auto bestIt = std::max_element(population.begin(), population.end(),
        [](const Individual& a, const Individual& b) {
            return a.fitness < b.fitness;
        });
    
    for (size_t i = 0; i < offspring.size() && i < population.size(); ++i) {
        unsigned int replaceIdx = dis(gen);
        
        // Tournament between random individual and current best
        if (population[replaceIdx].fitness < bestIt->fitness) {
            population[replaceIdx] = offspring[i];
        }
    }
    
    return population;
}

// Differential Evolution style replacement
std::vector<Individual> DifferentialEvolutionReplacement(std::vector<Individual>& population,
                                                        const std::vector<Individual>& offspring) {
    if (offspring.size() != population.size()) {
        std::cerr << "Error: Population and offspring sizes must match for DE replacement." << std::endl;
        return population;
    }
    
    for (size_t i = 0; i < population.size(); ++i) {
        // Each child competes directly with corresponding parent
        if (offspring[i].fitness > population[i].fitness) {
            population[i] = offspring[i];
        }
    }
    
    return population;
}

// ======================== MULTIOBJECTIVE SURVIVOR SELECTION ========================

// NSGA-II survivor selection
std::vector<MultiObjectiveIndividual> NSGAIISurvivorSelection(
    const std::vector<MultiObjectiveIndividual>& combined, unsigned int populationSize) {
    
    std::vector<MultiObjectiveIndividual> population = combined;
    auto fronts = fastNonDominatedSort(population);
    
    std::vector<MultiObjectiveIndividual> newPopulation;
    newPopulation.reserve(populationSize);
    
    for (const auto& front : fronts) {
        if (newPopulation.size() + front.size() <= populationSize) {
            // Add entire front
            for (int idx : front) {
                newPopulation.push_back(population[idx]);
            }
        } else {
            // Add part of front based on crowding distance
            std::vector<MultiObjectiveIndividual> frontIndividuals;
            for (int idx : front) {
                frontIndividuals.push_back(population[idx]);
            }
            
            calculateCrowdingDistance(frontIndividuals);
            
            // Sort by crowding distance (descending)
            std::sort(frontIndividuals.begin(), frontIndividuals.end(),
                [](const MultiObjectiveIndividual& a, const MultiObjectiveIndividual& b) {
                    return a.crowdingDistance > b.crowdingDistance;
                });
            
            // Add remaining individuals
            unsigned int remaining = populationSize - newPopulation.size();
            for (unsigned int i = 0; i < remaining; ++i) {
                newPopulation.push_back(frontIndividuals[i]);
            }
            break;
        }
    }
    
    return newPopulation;
}

// SPEA2 survivor selection (simplified version)
std::vector<MultiObjectiveIndividual> SPEA2SurvivorSelection(
    const std::vector<MultiObjectiveIndividual>& combined, unsigned int archiveSize) {
    
    std::vector<MultiObjectiveIndividual> archive;
    
    // First, add all non-dominated solutions
    for (const auto& ind : combined) {
        bool isDominated = false;
        for (const auto& other : combined) {
            if (dominates(other, ind)) {
                isDominated = true;
                break;
            }
        }
        if (!isDominated) {
            archive.push_back(ind);
        }
    }
    
    // If archive is too large, trim it
    while (archive.size() > archiveSize) {
        // Calculate distances between all pairs
        std::vector<std::vector<double>> distances(archive.size(), std::vector<double>(archive.size()));
        
        for (size_t i = 0; i < archive.size(); ++i) {
            for (size_t j = i + 1; j < archive.size(); ++j) {
                double dist = 0.0;
                for (size_t k = 0; k < archive[i].objectives.size(); ++k) {
                    dist += std::pow(archive[i].objectives[k] - archive[j].objectives[k], 2);
                }
                distances[i][j] = distances[j][i] = std::sqrt(dist);
            }
        }
        
        // Find individual with smallest distance to nearest neighbor
        double minDistance = std::numeric_limits<double>::max();
        size_t removeIdx = 0;
        
        for (size_t i = 0; i < archive.size(); ++i) {
            double nearestDistance = std::numeric_limits<double>::max();
            for (size_t j = 0; j < archive.size(); ++j) {
                if (i != j && distances[i][j] < nearestDistance) {
                    nearestDistance = distances[i][j];
                }
            }
            if (nearestDistance < minDistance) {
                minDistance = nearestDistance;
                removeIdx = i;
            }
        }
        
        archive.erase(archive.begin() + removeIdx);
    }
    
    return archive;
}

// ======================== UTILITY FUNCTIONS ========================

// Calculate Euclidean distance between two individuals
double calculateDistance(const Individual& ind1, const Individual& ind2) {
    if (ind1.genes.size() != ind2.genes.size()) {
        return std::numeric_limits<double>::max();
    }
    
    double distance = 0.0;
    for (size_t i = 0; i < ind1.genes.size(); ++i) {
        distance += std::pow(ind1.genes[i] - ind2.genes[i], 2);
    }
    return std::sqrt(distance);
}

// Check if individual1 dominates individual2 (for multiobjective)
bool dominates(const MultiObjectiveIndividual& ind1, const MultiObjectiveIndividual& ind2) {
    bool atLeastOneSmaller = false;
    
    for (size_t i = 0; i < ind1.objectives.size(); ++i) {
        if (ind1.objectives[i] > ind2.objectives[i]) {
            return false;  // ind1 is worse in at least one objective
        }
        if (ind1.objectives[i] < ind2.objectives[i]) {
            atLeastOneSmaller = true;
        }
    }
    
    return atLeastOneSmaller;
}

// Calculate crowding distance for NSGA-II
void calculateCrowdingDistance(std::vector<MultiObjectiveIndividual>& front) {
    size_t frontSize = front.size();
    
    // Initialize crowding distance
    for (auto& ind : front) {
        ind.crowdingDistance = 0.0;
    }
    
    if (frontSize <= 2) {
        for (auto& ind : front) {
            ind.crowdingDistance = std::numeric_limits<double>::max();
        }
        return;
    }
    
    size_t numObjectives = front[0].objectives.size();
    
    for (size_t obj = 0; obj < numObjectives; ++obj) {
        // Sort by objective
        std::sort(front.begin(), front.end(),
            [obj](const MultiObjectiveIndividual& a, const MultiObjectiveIndividual& b) {
                return a.objectives[obj] < b.objectives[obj];
            });
        
        // Set boundary points to infinity
        front[0].crowdingDistance = std::numeric_limits<double>::max();
        front[frontSize - 1].crowdingDistance = std::numeric_limits<double>::max();
        
        // Calculate crowding distance for intermediate points
        double objectiveRange = front[frontSize - 1].objectives[obj] - front[0].objectives[obj];
        
        if (objectiveRange > 0) {
            for (size_t i = 1; i < frontSize - 1; ++i) {
                front[i].crowdingDistance += 
                    (front[i + 1].objectives[obj] - front[i - 1].objectives[obj]) / objectiveRange;
            }
        }
    }
}

// Fast non-dominated sorting for NSGA-II
std::vector<std::vector<int>> fastNonDominatedSort(std::vector<MultiObjectiveIndividual>& population) {
    std::vector<std::vector<int>> fronts;
    std::vector<int> currentFront;
    
    // Initialize domination counts and dominated solutions
    for (size_t i = 0; i < population.size(); ++i) {
        population[i].dominationCount = 0;
        population[i].dominatedSolutions.clear();
        
        for (size_t j = 0; j < population.size(); ++j) {
            if (i != j) {
                if (dominates(population[i], population[j])) {
                    population[i].dominatedSolutions.push_back(j);
                } else if (dominates(population[j], population[i])) {
                    population[i].dominationCount++;
                }
            }
        }
        
        if (population[i].dominationCount == 0) {
            population[i].rank = 0;
            currentFront.push_back(i);
        }
    }
    
    fronts.push_back(currentFront);
    
    int frontNumber = 0;
    while (!fronts[frontNumber].empty()) {
        std::vector<int> nextFront;
        
        for (int i : fronts[frontNumber]) {
            for (int j : population[i].dominatedSolutions) {
                population[j].dominationCount--;
                if (population[j].dominationCount == 0) {
                    population[j].rank = frontNumber + 1;
                    nextFront.push_back(j);
                }
            }
        }
        
        frontNumber++;
        fronts.push_back(nextFront);
    }
    
    fronts.pop_back();  // Remove empty last front
    return fronts;
}