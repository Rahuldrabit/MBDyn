/* MBDyn UDE: genetic_algorithm
 *
 * Complete GA implementation in the module:
 * - libga.so: ONLY initializes population
 * - libcons.so: ONLY provides fitness function  
 * - This module: handles ALL GA operations (selection, crossover, mutation, evolution)
 */

#include "mbconfig.h"

#include <dlfcn.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <random>
#include <memory>

#include "dataman.h"
#include "userelem.h"
#include "drive_.h"

// Include operator implementations directly in module
#include "crossover/crossover.h"
#include "mutation/mutation.h"
#include "selection-operator/selection-operator.h"

// libga.so minimal API (population initialization only)
typedef void* (*GAInitPopFunc)(int pop_size, int chrom_len, const double* lower, const double* upper);
typedef void  (*GAGetIndividualFunc)(void* ctx, int index, double* individual);
typedef void  (*GASetIndividualFunc)(void* ctx, int index, const double* individual);
typedef int   (*GAGetPopSizeFunc)(void* ctx);
typedef int   (*GAGetChromLenFunc)(void* ctx);
typedef void  (*GACleanupFunc)(void* ctx);

// libcons.so fitness API (fitness evaluation only)
typedef double (*EvaluateFitnessFunc)(const double* genes, int n_genes, const double* inputs, int n_inputs);
typedef double (*EvaluateConstraintFunc)(const double* genes, int n_genes);



class GeneticAlgorithm : public UserDefinedElem {
private:
    // GA parameters
    int population_size;
    int generations;
    int current_generation;
    int chromosome_length;
    
    // Operator configuration
    std::string crossover_name;
    std::string mutation_name;
    std::string selection_name;
    double mutation_rate;
    double crossover_rate;
    double elite_ratio;
    
    // Bounds
    std::vector<double> lower_bounds;
    std::vector<double> upper_bounds;
    
    // Input/output handling
    struct InputDC {
        DriveCaller* pDC;
    };
    std::vector<InputDC> m_inputs;
    std::vector<doublereal> m_inputVals;
    std::vector<doublereal> m_outputs;
    std::vector<integer> m_outputLabels;
    
    // GA state (MODULE manages all this)
    std::vector<std::vector<double>> population;
    std::vector<double> fitness_values;
    std::vector<double> best_individual;
    double best_fitness;
    
    // RNG
    std::mt19937 rng;
    std::uniform_real_distribution<double> uniform_dist;
    
    // Operator instances (MODULE owns these)
    std::unique_ptr<TwoPointCrossover> two_point_crossover;
    std::unique_ptr<OnePointCrossover> one_point_crossover;
    std::unique_ptr<UniformCrossover> uniform_crossover;
    std::unique_ptr<BlendCrossover> blend_crossover;
    std::unique_ptr<MutationOperators> mutation_ops;
    
    // Library handles (minimal usage)
    void* gaLibHandle;
    void* consLibHandle;
    
    // libga.so functions (initialization only)
    GAInitPopFunc gaInitPop;
    GAGetIndividualFunc gaGetIndividual;
    GASetIndividualFunc gaSetIndividual;
    GAGetPopSizeFunc gaGetPopSize;
    GAGetChromLenFunc gaGetChromLen;
    GACleanupFunc gaCleanup;
    
    // libcons.so functions (fitness only)
    EvaluateFitnessFunc evaluateFitness;
    EvaluateConstraintFunc evaluateConstraint;
    
    void* gaCtx; // Population context from libga.so
    
    // Internal GA methods (MODULE does all the work)
    void evaluatePopulation();
    void evolveGeneration();
    std::vector<int> selectElite();
    Individual selectParent();
    void performCrossover(std::vector<std::vector<double>>& new_pop, int& offspring_count);
    void applyMutation(std::vector<double>& individual);

public:
    GeneticAlgorithm(unsigned uLabel, const DofOwner *pDO,
        DataManager* pDM, MBDynParser& HP);
    virtual ~GeneticAlgorithm(void);

    virtual void Output(OutputHandler& OH) const;
    virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;
    VariableSubMatrixHandler& AssJac(VariableSubMatrixHandler& WorkMat,
        doublereal dCoef, const VectorHandler& XCurr,
        const VectorHandler& XPrimeCurr);
    SubVectorHandler& AssRes(SubVectorHandler& WorkVec,
        doublereal dCoef, const VectorHandler& XCurr, 
        const VectorHandler& XPrimeCurr);
    unsigned int iGetNumPrivData(void) const;
    virtual unsigned int iGetPrivDataIdx(const char *s) const;
    virtual doublereal dGetPrivData(unsigned int i) const;
};

// Constructor
GeneticAlgorithm::GeneticAlgorithm(
    unsigned uLabel, const DofOwner *pDO,
    DataManager* pDM, MBDynParser& HP)
: UserDefinedElem(uLabel, pDO), 
  gaLibHandle(nullptr), consLibHandle(nullptr), gaCtx(nullptr),
  current_generation(0), best_fitness(-1e30),
  rng(std::random_device{}()), uniform_dist(0.0, 1.0)
{
    // Defaults
    population_size = 50;
    generations = 100;
    chromosome_length = 5;
    mutation_rate = 0.1;
    crossover_rate = 0.8;
    elite_ratio = 0.1;
    crossover_name = "two_point";
    mutation_name = "uniform";
    selection_name = "tournament";
    
    std::string gaLibPath = "libga.so";
    std::string consLibPath = "libcons.so";

    // Parse inputs
    if (HP.IsKeyWord("inputs" "number")) {
        integer nIn = HP.GetInt();
        if (nIn < 0) {
            throw ErrGeneric(MBDYN_EXCEPT_ARGS, "genetic_algorithm: inputs number must be >= 0");
        }
        m_inputs.resize(static_cast<size_t>(nIn));
        m_inputVals.resize(static_cast<size_t>(nIn), 0.);
        for (integer i = 0; i < nIn; ++i) {
            m_inputs[static_cast<size_t>(i)].pDC = HP.GetDriveCaller();
        }
    }

    // Parse library paths
    if (HP.IsKeyWord("genetic" "algorithm")) {
        gaLibPath = HP.GetString();
    }
    if (HP.IsKeyWord("constraint" "function")) {
        consLibPath = HP.GetString();
    }

    // Parse GA parameters
    if (HP.IsKeyWord("population")) {
        population_size = HP.GetInt();
    }
    if (HP.IsKeyWord("generation")) {
        generations = HP.GetInt();
    }
    if (HP.IsKeyWord("crossover")) {
        crossover_name = HP.GetString();
    }
    if (HP.IsKeyWord("mutation")) {
        mutation_name = HP.GetString();
    }
    if (HP.IsKeyWord("selection")) {
        selection_name = HP.GetString();
    }
    if (HP.IsKeyWord("mutation" "rate")) {
        mutation_rate = HP.GetReal();
    }
    if (HP.IsKeyWord("crossover" "rate")) {
        crossover_rate = HP.GetReal();
    }
    if (HP.IsKeyWord("elite" "ratio")) {
        elite_ratio = HP.GetReal();
    }

    // Parse outputs
    if (HP.IsKeyWord("output" "number")) {
        integer nOut = HP.GetInt();
        if (nOut < 0) {
            throw ErrGeneric(MBDYN_EXCEPT_ARGS, "genetic_algorithm: output number must be >= 0");
        }
        chromosome_length = nOut; // Chromosome represents outputs
        m_outputs.assign(static_cast<size_t>(nOut), 0.);
        m_outputLabels.resize(static_cast<size_t>(nOut));
        
        for (integer i = 0; i < nOut; ++i) {
            m_outputLabels[static_cast<size_t>(i)] = HP.GetInt();
        }
    }

    // Initialize bounds (default [-10, 10])
    lower_bounds.assign(chromosome_length, -10.0);
    upper_bounds.assign(chromosome_length, 10.0);

    // Load libraries
    gaLibHandle = dlopen(gaLibPath.c_str(), RTLD_LAZY);
    if (!gaLibHandle) {
        throw ErrGeneric(MBDYN_EXCEPT_ARGS, ("Cannot load " + gaLibPath + ": " + dlerror()).c_str());
    }

    consLibHandle = dlopen(consLibPath.c_str(), RTLD_LAZY);
    if (!consLibHandle) {
        silent_cerr("GeneticAlgorithm(" << GetLabel() << "): warning - constraint library not loaded" << std::endl);
    }

    // Resolve libga.so symbols (initialization only)
    gaInitPop = (GAInitPopFunc)dlsym(gaLibHandle, "ga_init_population");
    gaGetIndividual = (GAGetIndividualFunc)dlsym(gaLibHandle, "ga_get_individual");
    gaSetIndividual = (GASetIndividualFunc)dlsym(gaLibHandle, "ga_set_individual");
    gaGetPopSize = (GAGetPopSizeFunc)dlsym(gaLibHandle, "ga_get_population_size");
    gaGetChromLen = (GAGetChromLenFunc)dlsym(gaLibHandle, "ga_get_chromosome_length");
    gaCleanup = (GACleanupFunc)dlsym(gaLibHandle, "ga_cleanup");

    if (!gaInitPop) {
        throw ErrGeneric(MBDYN_EXCEPT_ARGS, "GA population init symbol missing");
    }

    // Resolve libcons.so symbols (fitness only)
    if (consLibHandle) {
        evaluateFitness = (EvaluateFitnessFunc)dlsym(consLibHandle, "evaluate_fitness");
        evaluateConstraint = (EvaluateConstraintFunc)dlsym(consLibHandle, "evaluate_constraint");
        if (!evaluateFitness) {
            silent_cerr("GeneticAlgorithm(" << GetLabel() << "): warning - fitness function not found" << std::endl);
        }
    }

    // Initialize population via libga.so (ONLY initialization)
    gaCtx = gaInitPop(population_size, chromosome_length, 
                      lower_bounds.data(), upper_bounds.data());
    
    // Copy population to MODULE storage (MODULE manages it from now on)
    population.resize(population_size);
    for (int i = 0; i < population_size; ++i) {
        population[i].resize(chromosome_length);
        if (gaGetIndividual) {
            gaGetIndividual(gaCtx, i, population[i].data());
        }
    }
    
    fitness_values.resize(population_size, 0.0);
    best_individual.resize(chromosome_length, 0.0);

    // Create operator instances (MODULE owns these)
    two_point_crossover = std::make_unique<TwoPointCrossover>();
    one_point_crossover = std::make_unique<OnePointCrossover>();
    uniform_crossover = std::make_unique<UniformCrossover>();
    blend_crossover = std::make_unique<BlendCrossover>();
    mutation_ops = std::make_unique<MutationOperators>();

    std::cout << "GeneticAlgorithm initialized (module-driven):" << std::endl;
    std::cout << "  Population: " << population_size << ", Generations: " << generations << std::endl;
    std::cout << "  Operators: " << crossover_name << "/" << mutation_name << "/" << selection_name << std::endl;
    std::cout << "  Rates: crossover=" << crossover_rate << ", mutation=" << mutation_rate << std::endl;
    std::cout << "  libga.so: population initialization ONLY" << std::endl;
    std::cout << "  libcons.so: fitness evaluation ONLY" << std::endl;
    std::cout << "  module: ALL GA operations (selection, crossover, mutation)" << std::endl;
}

// Destructor
GeneticAlgorithm::~GeneticAlgorithm(void)
{
    if (gaCleanup && gaCtx) {
        gaCleanup(gaCtx);
    }
    if (gaLibHandle) {
        dlclose(gaLibHandle);
    }
    if (consLibHandle) {
        dlclose(consLibHandle);
    }
    
    // Clean up drive callers
    for (auto& input : m_inputs) {
        if (input.pDC) {
            delete input.pDC;
        }
    }
}

// MODULE METHODS - All GA logic happens here

void GeneticAlgorithm::evaluatePopulation() {
    for (int i = 0; i < population_size; ++i) {
        if (evaluateFitness) {
            // Use libcons.so ONLY for fitness
            fitness_values[i] = evaluateFitness(population[i].data(), chromosome_length,
                                               m_inputVals.data(), m_inputVals.size());
        } else {
            // Fallback: simple sphere function
            double sum = 0.0;
            for (double gene : population[i]) {
                sum += gene * gene;
            }
            fitness_values[i] = -sum;
        }
        
        // Track best
        if (fitness_values[i] > best_fitness) {
            best_fitness = fitness_values[i];
            best_individual = population[i];
            // Update outputs
            for (size_t j = 0; j < m_outputs.size() && j < best_individual.size(); ++j) {
                m_outputs[j] = best_individual[j];
            }
        }
    }
}

std::vector<int> GeneticAlgorithm::selectElite() {
    int elite_count = std::max(1, static_cast<int>(population_size * elite_ratio));
    std::vector<int> indices(population_size);
    std::iota(indices.begin(), indices.end(), 0);
    
    std::partial_sort(indices.begin(), indices.begin() + elite_count, indices.end(),
                     [this](int a, int b) { return fitness_values[a] > fitness_values[b]; });
    
    indices.resize(elite_count);
    return indices;
}

Individual GeneticAlgorithm::selectParent() {
    // Tournament selection (configurable via selection_name)
    int tournament_size = 3;
    int best_idx = uniform_dist(rng) * population_size;
    double best_fit = fitness_values[best_idx];
    
    for (int i = 1; i < tournament_size; ++i) {
        int idx = uniform_dist(rng) * population_size;
        if (fitness_values[idx] > best_fit) {
            best_idx = idx;
            best_fit = fitness_values[idx];
        }
    }
    
    Individual parent;
    parent.fitness = fitness_values[best_idx];
    parent.genes = population[best_idx];
    return parent;
}

void GeneticAlgorithm::performCrossover(std::vector<std::vector<double>>& new_pop, int& offspring_count) {
    while (offspring_count < population_size) {
        if (uniform_dist(rng) < crossover_rate) {
            Individual parent1 = selectParent();
            Individual parent2 = selectParent();
            
            std::pair<RealVector, RealVector> offspring;
            
            // MODULE applies selected crossover operator
            if (crossover_name == "two_point") {
                offspring = two_point_crossover->crossover(parent1.genes, parent2.genes);
            } else if (crossover_name == "one_point") {
                offspring = one_point_crossover->crossover(parent1.genes, parent2.genes);
            } else if (crossover_name == "uniform") {
                offspring = uniform_crossover->crossover(parent1.genes, parent2.genes);
            } else if (crossover_name == "blend") {
                offspring = blend_crossover->crossover(parent1.genes, parent2.genes);
            } else {
                offspring = two_point_crossover->crossover(parent1.genes, parent2.genes);
            }
            
            if (offspring_count < population_size) {
                new_pop[offspring_count++] = offspring.first;
            }
            if (offspring_count < population_size) {
                new_pop[offspring_count++] = offspring.second;
            }
        } else {
            Individual parent = selectParent();
            new_pop[offspring_count++] = parent.genes;
        }
    }
}

void GeneticAlgorithm::applyMutation(std::vector<double>& individual) {
    // MODULE applies selected mutation operator
    if (mutation_name == "uniform") {
        mutation_ops->uniformMutation(individual, mutation_rate, lower_bounds, upper_bounds);
    } else if (mutation_name == "gaussian") {
        double sigma = 0.1;
        mutation_ops->gaussianMutation(individual, mutation_rate, sigma, lower_bounds, upper_bounds);
    } else {
        mutation_ops->uniformMutation(individual, mutation_rate, lower_bounds, upper_bounds);
    }
}

void GeneticAlgorithm::evolveGeneration() {
    // MODULE orchestrates entire evolution
    evaluatePopulation();
    
    // Select elite individuals
    std::vector<int> elite_indices = selectElite();
    
    // Create new population
    std::vector<std::vector<double>> new_population(population_size);
    int offspring_count = 0;
    
    // Copy elites
    for (int idx : elite_indices) {
        new_population[offspring_count++] = population[idx];
    }
    
    // Generate offspring via crossover
    performCrossover(new_population, offspring_count);
    
    // Apply mutation (skip elites)
    for (size_t i = elite_indices.size(); i < static_cast<size_t>(population_size); ++i) {
        applyMutation(new_population[i]);
    }
    
    // Replace old population
    population = std::move(new_population);
    current_generation++;
    
    // Print progress
    if (current_generation % 10 == 0 || current_generation == 1) {
        std::cout << "Generation " << current_generation << ": Best fitness = " << best_fitness << std::endl;
    }
}

void GeneticAlgorithm::Output(OutputHandler& OH) const {
    OH << "Generation " << current_generation << ", Best fitness = " << best_fitness << "\n";
}

void GeneticAlgorithm::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const {
    *piNumRows = 0;
    *piNumCols = 0;
}

VariableSubMatrixHandler& GeneticAlgorithm::AssJac(
    VariableSubMatrixHandler& WorkMat, doublereal, 
    const VectorHandler&, const VectorHandler&) {
    WorkMat.SetNullMatrix();
    return WorkMat;
}

SubVectorHandler& GeneticAlgorithm::AssRes(
    SubVectorHandler& WorkVec, doublereal,
    const VectorHandler&, const VectorHandler&) {
    WorkVec.ResizeReset(0);
    
    // Sample input drives
    for (size_t i = 0; i < m_inputs.size(); ++i) {
        if (m_inputs[i].pDC != nullptr) {
            m_inputVals[i] = m_inputs[i].pDC->dGet();
        }
    }
    
    // MODULE runs one generation per timestep (or configure differently)
    if (current_generation < generations) {
        evolveGeneration();
    }
    
    return WorkVec;
}

unsigned int GeneticAlgorithm::iGetNumPrivData(void) const {
    // Expose 1 (fitness) + number of outputs
    return static_cast<unsigned int>(1 + m_outputs.size());
}

unsigned int GeneticAlgorithm::iGetPrivDataIdx(const char *s) const {
    if (s == nullptr) {
        throw ErrGeneric(MBDYN_EXCEPT_ARGS, "genetic_algorithm: null private data name");
    }
    if (std::strcmp(s, "fitness") == 0) {
        return 1U;
    }
    if (std::strncmp(s, "output[", 7) == 0) {
        const char* p = s + 7;
        char* endp = nullptr;
        long idx = std::strtol(p, &endp, 10);
        if (endp && *endp == ']' && idx >= 1 && static_cast<size_t>(idx) <= m_outputs.size()) {
            return static_cast<unsigned int>(1 + idx);
        }
    }
    throw ErrGeneric(MBDYN_EXCEPT_ARGS, "genetic_algorithm: unknown private data name");
}

doublereal GeneticAlgorithm::dGetPrivData(unsigned int i) const {
    if (i == 1U) {
        return best_fitness;
    }
    unsigned int outIdx = (i >= 2U) ? (i - 2U) : UINT_MAX;
    if (outIdx < m_outputs.size()) {
        return m_outputs[outIdx];
    }
    throw ErrGeneric(MBDYN_EXCEPT_ARGS, "genetic_algorithm: private data index out of range");
}

// Module init
extern "C" int module_init(const char *module_name, void *pdm, void *php) {
    UserDefinedElemRead *rf = new UDERead<GeneticAlgorithm>;
    if (!SetUDE("genetic_algorithm", rf)) {
        delete rf;
        return -1;
    }
    UserDefinedElemRead *rf2 = new UDERead<GeneticAlgorithm>;
    if (!SetUDE("genetic algorithm optimization", rf2)) {
        delete rf2;
    }
    return 0;
}
