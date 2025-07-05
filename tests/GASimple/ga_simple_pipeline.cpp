#include <iostream>
#include <vector>
#include <random>
#include <algorithm>

struct Individual {
    std::vector<double> genes;
    double fitness{};
};

// Simple fitness: negative sum of squared error to target 1.0 for each gene
static double
fitnessFunction(const std::vector<double>& genes)
{
    double sum = 0.0;
    for (double g : genes) {
        double d = g - 1.0; // target value
        sum += d * d;
    }
    return -sum; // larger is better
}

// Tournament selection
static const Individual&
tournamentSelection(const std::vector<Individual>& pop,
                    std::mt19937& rng,
                    unsigned int tsize)
{
    std::uniform_int_distribution<size_t> dist(0, pop.size() - 1);
    size_t best = dist(rng);
    for (unsigned i = 1; i < tsize; ++i) {
        size_t idx = dist(rng);
        if (pop[idx].fitness > pop[best].fitness) {
            best = idx;
        }
    }
    return pop[best];
}

// Single point crossover
static std::pair<Individual, Individual>
singlePointCrossover(const Individual& p1, const Individual& p2,
                     std::mt19937& rng)
{
    std::uniform_int_distribution<size_t> dist(1, p1.genes.size() - 1);
    size_t point = dist(rng);
    Individual c1 = p1;
    Individual c2 = p2;
    for (size_t i = point; i < p1.genes.size(); ++i) {
        std::swap(c1.genes[i], c2.genes[i]);
    }
    return {c1, c2};
}

// Gaussian mutation
static void
mutate(Individual& ind, std::mt19937& rng, double prob, double sigma)
{
    std::uniform_real_distribution<double> pdist(0.0, 1.0);
    std::normal_distribution<double> ndist(0.0, sigma);
    for (double& g : ind.genes) {
        if (pdist(rng) < prob) {
            g += ndist(rng);
        }
    }
}

// Simple hill climbing: attempt small step; keep if fitness improves
static void
hillClimb(Individual& ind, std::mt19937& rng, double stepSigma)
{
    std::normal_distribution<double> ndist(0.0, stepSigma);
    Individual candidate = ind;
    for (double& g : candidate.genes) {
        g += ndist(rng);
    }
    candidate.fitness = fitnessFunction(candidate.genes);
    if (candidate.fitness > ind.fitness) {
        ind = candidate;
    }
}

int
main()
{
    const unsigned geneSize = 5;
    const unsigned popSize = 6;
    const unsigned generations = 10;
    const unsigned tournamentSize = 3;
    const double mutationProb = 0.2;
    const double sigma = 0.1;
    const double hillSigma = 0.05;

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> initDist(-1.0, 3.0);

    std::vector<Individual> population(popSize);
    for (auto& ind : population) {
        ind.genes.resize(geneSize);
        for (double& g : ind.genes) {
            g = initDist(rng);
        }
        ind.fitness = fitnessFunction(ind.genes);
    }

    for (unsigned gen = 0; gen < generations; ++gen) {
        const Individual& parent1 = tournamentSelection(population, rng, tournamentSize);
        const Individual& parent2 = tournamentSelection(population, rng, tournamentSize);

        auto children = singlePointCrossover(parent1, parent2, rng);
        mutate(children.first, rng, mutationProb, sigma);
        mutate(children.second, rng, mutationProb, sigma);

        children.first.fitness = fitnessFunction(children.first.genes);
        children.second.fitness = fitnessFunction(children.second.genes);

        hillClimb(children.first, rng, hillSigma);
        hillClimb(children.second, rng, hillSigma);

        std::sort(population.begin(), population.end(),
                  [](const Individual& a, const Individual& b) {
                      return a.fitness > b.fitness;
                  });

        population[popSize - 1] = children.first;
        population[popSize - 2] = children.second;

        double bestFitness = population[0].fitness;
        std::cout << "Generation " << gen << ": best fitness = "
                  << bestFitness << std::endl;
    }
    return 0;
}

