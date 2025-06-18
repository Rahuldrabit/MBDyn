#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>

// Individual structure for GA population
struct Individual {
    std::vector<double> genes;
    double fitness;
};

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
std::vector<unsigned int> RouletteWheelSelection(std::vector<Individual>& Population, unsigned int NumSelections = 1) {
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
std::vector<unsigned int> RankSelection(std::vector<Individual>& Population, unsigned int NumSelections = 1) {
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