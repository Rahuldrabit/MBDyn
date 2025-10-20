/* MBDyn UDE: genetic_algorithm
 *
 * Complete GA implementation in the module:
 * - libga.so: ONLY initializes population
 * - libcons.so: ONLY provides fitness function  
 * - This module: handles ALL GA operations (selection, crossover, mutation, evolution)
 */

//#include "mbconfig.h"

#include <dlfcn.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <random>
#include <memory>
#include <unordered_map>
#include <limits>
#include <numeric>

#include "dataman.h"
#include "userelem.h"
#include "drive_.h"

// Include operator implementations directly in module
#include "crossover/crossover.h"
#include "mutation/mutation.h"
#include "selection-operator/selection-operator.h"

// libga.so minimal API (population initialization only)
typedef void* (*GACtxCreateFunc)(int pop_size, int chrom_len, const double* lower, const double* upper, unsigned int seed);
typedef void  (*GACtxDestroyFunc)(void* ctx);
typedef int   (*GARandomizePopulationFunc)(void* ctx, double sigma_fraction);
typedef int   (*GARegisterEnvPtrsFunc)(void* ctx, double* inputs, int nInputs, double* outputs, int nOutputs, double* desired);
typedef int   (*GAEvaluatePopulationFunc)(void* ctx);
typedef int   (*GABestIndexFunc)(void* ctx);
typedef void  (*GAGetBestIndividualFunc)(void* ctx, double* out_genes);
typedef int   (*GAGetTopNFunc)(void* ctx, int N, double* out_genes);

// libcons.so fitness API (fitness evaluation only)
typedef double (*EvaluateFitnessFunc)(const double* genes, int n_genes, const double* inputs, int n_inputs);



class GeneticAlgorithm : public UserDefinedElem {
private:
    // GA parameters
    int population_size;
    int generations;
    int current_generation;
    int chromosome_length;
    int return_best_population;
    
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
    GACtxCreateFunc gaCtxCreate;
    GACtxDestroyFunc gaCtxDestroy;
    GARandomizePopulationFunc gaRandomizePopulation;
    GARegisterEnvPtrsFunc gaRegisterEnvPtrs;
    GAEvaluatePopulationFunc gaEvaluatePopulation;
    GABestIndexFunc gaBestIndex;
    GAGetBestIndividualFunc gaGetBestIndividual;
    GAGetTopNFunc gaGetTopN;
    
    // libcons.so functions (fitness only)
    EvaluateFitnessFunc evaluateFitness;
    
    void* gaCtx; // Population context from libga.so
    
    // Variable registry for optimization variables
    VariableRegistry var_registry;
    
    // Internal GA methods (MODULE does all the work)
    void evaluatePopulation();
    void evolveGeneration();
    std::vector<int> selectElite();
    Individual selectParent();
    void performCrossover(std::vector<std::vector<double>>& new_pop, int& offspring_count);
    void applyMutation(std::vector<double>& individual);
    
    // Apply best solution to registered variables
    void applyBestSolution();

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
    
    // Register a variable for optimization
    bool registerVariable(const std::string& name, double* ptr, 
                          double min_value, double max_value,
                          const std::string& description = "");
                          
    // Register a variable from a shared library
    bool registerSharedVariable(const std::string& library_path,
                               const std::string& symbol_name,
                               const std::string& var_name,
                               double min_value, double max_value,
                               const std::string& description = "");
};

// Variable Registry to dynamically link optimization variables
class VariableRegistry {
private:
    // Structure to hold variable information
    struct VarInfo {
        double* ptr;      // Pointer to the actual variable
        double min_value; // Minimum allowed value
        double max_value; // Maximum allowed value
        std::string description; // Optional description
        void* library_handle; // Handle to the shared library (nullptr for direct variables)
    };

    // Map of variable names to their pointers and metadata
    std::unordered_map<std::string, VarInfo> registry;
    
    // Current values (for optimization/temporary storage)
    std::unordered_map<std::string, double> current_values;
    
    // List of variable names in registration order (for consistent indexing)
    std::vector<std::string> var_names;
    
    // Map of loaded shared libraries
    std::unordered_map<std::string, void*> loaded_libraries;

public:
    VariableRegistry() {}
    
    ~VariableRegistry() {
        // Close all opened libraries
        for (auto& lib : loaded_libraries) {
            if (lib.second) {
                dlclose(lib.second);
            }
        }
    }

    // Register a new variable with direct pointer
    bool registerVariable(const std::string& name, double* ptr, 
                         double min_value, double max_value,
                         const std::string& description = "") {
        if (registry.find(name) != registry.end()) {
            return false; // Already exists
        }
        
        VarInfo info{ptr, min_value, max_value, description, nullptr};
        registry[name] = info;
        var_names.push_back(name);
        
        // Initialize current value
        if (ptr) {
            current_values[name] = *ptr;
        } else {
            current_values[name] = 0.0;
        }
        
        return true;
    }
    
    // Load a shared library and register its variables
    bool loadSharedLibrary(const std::string& library_path) {
        // Check if already loaded
        if (loaded_libraries.find(library_path) != loaded_libraries.end()) {
            return true; // Already loaded
        }
        
        // Load the library
        void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "Error loading library: " << dlerror() << std::endl;
            return false;
        }
        
        loaded_libraries[library_path] = handle;
        return true;
    }
    
    // Register a variable from a shared library by symbol name
    bool registerSharedVariable(const std::string& library_path, 
                               const std::string& symbol_name,
                               const std::string& var_name,
                               double min_value, double max_value,
                               const std::string& description = "") {
        // Ensure library is loaded
        if (!loadSharedLibrary(library_path)) {
            return false;
        }
        
        void* lib_handle = loaded_libraries[library_path];
        
        // Look up the symbol in the shared library
        double* ptr = static_cast<double*>(dlsym(lib_handle, symbol_name.c_str()));
        if (!ptr) {
            std::cerr << "Error finding symbol " << symbol_name 
                      << " in library " << library_path 
                      << ": " << dlerror() << std::endl;
            return false;
        }
        
        // Register the variable with the resolved pointer
        if (registry.find(var_name) != registry.end()) {
            return false; // Already exists
        }
        
        VarInfo info{ptr, min_value, max_value, description, lib_handle};
        registry[var_name] = info;
        var_names.push_back(var_name);
        
        // Initialize current value
        current_values[var_name] = *ptr;
        
        std::cout << "Registered shared variable: " << var_name 
                  << " from " << library_path << "::" << symbol_name 
                  << " (current value: " << *ptr << ")" << std::endl;
        
        return true;
    }
    
    // Get number of registered variables
    size_t size() const {
        return registry.size();
    }
    
    // Check if a variable exists
    bool hasVariable(const std::string& name) const {
        return registry.find(name) != registry.end();
    }
    
    // Get variable value
    double getValue(const std::string& name) const {
        auto it = current_values.find(name);
        if (it != current_values.end()) {
            return it->second;
        }
        return 0.0;
    }
    
    // Set variable value (doesn't immediately update the actual variable)
    bool setValue(const std::string& name, double value) {
        auto it = registry.find(name);
        if (it == registry.end()) {
            return false;
        }
        
        // Clamp to bounds
        if (value < it->second.min_value) value = it->second.min_value;
        if (value > it->second.max_value) value = it->second.max_value;
        
        current_values[name] = value;
        return true;
    }
    
    // Get variable by index (for population encoding/decoding)
    std::string getNameByIndex(size_t index) const {
        if (index < var_names.size()) {
            return var_names[index];
        }
        return "";
    }
    
    // Get bounds for all variables
    void getBounds(std::vector<double>& lower, std::vector<double>& upper) const {
        lower.resize(var_names.size());
        upper.resize(var_names.size());
        
        for (size_t i = 0; i < var_names.size(); ++i) {
            const auto& info = registry.at(var_names[i]);
            lower[i] = info.min_value;
            upper[i] = info.max_value;
        }
    }
    
    // Apply current values to the actual variables
    void applyValues() {
        for (const auto& name : var_names) {
            auto& info = registry[name];
            if (info.ptr) {
                *(info.ptr) = current_values[name];
            }
        }
    }
    
    // Encode current values into a chromosome
    void encodeToChromosome(std::vector<double>& chromosome) const {
        chromosome.resize(var_names.size());
        for (size_t i = 0; i < var_names.size(); ++i) {
            chromosome[i] = current_values.at(var_names[i]);
        }
    }
    
    // Decode chromosome into current values
    void decodeFromChromosome(const std::vector<double>& chromosome) {
        for (size_t i = 0; i < chromosome.size() && i < var_names.size(); ++i) {
            double value = chromosome[i];
            const auto& info = registry[var_names[i]];
            
            // Clamp to bounds
            if (value < info.min_value) value = info.min_value;
            if (value > info.max_value) value = info.max_value;
            
            current_values[var_names[i]] = value;
        }
    }
    
    // Get variable info for debugging/output
    std::vector<std::pair<std::string, std::pair<double, double>>> getVariableInfo() const {
        std::vector<std::pair<std::string, std::pair<double, double>>> result;
        for (const auto& name : var_names) {
            const auto& info = registry.at(name);
            result.push_back({name, {info.min_value, info.max_value}});
        }
        return result;
    }
};

// Constructor
GeneticAlgorithm::GeneticAlgorithm(
    unsigned uLabel, const DofOwner *pDO,
    DataManager* pDM, MBDynParser& HP)
: UserDefinedElem(uLabel, pDO), 
        gaLibHandle(nullptr), consLibHandle(nullptr),
        gaCtxCreate(nullptr), gaCtxDestroy(nullptr),
        gaRandomizePopulation(nullptr), gaRegisterEnvPtrs(nullptr),
        gaEvaluatePopulation(nullptr), gaBestIndex(nullptr),
        gaGetBestIndividual(nullptr), gaGetTopN(nullptr),
        evaluateFitness(nullptr),
        gaCtx(nullptr),
    current_generation(0), best_fitness(-1e30),
    return_best_population(1),
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
    if (HP.IsKeyWord("population_upper")) {
        double val = HP.GetReal();
        std::fill(upper_bounds.begin(), upper_bounds.end(), val);
    }
    if (HP.IsKeyWord("population_lower")) {
        double val = HP.GetReal();
        std::fill(lower_bounds.begin(), lower_bounds.end(), val);
    }
    if (HP.IsKeyWord("return_best_population")) {
        return_best_population = HP.GetInt();
    }
    
    // Parse variable registry entries
    if (HP.IsKeyWord("variables")) {
        integer nVars = HP.GetInt();
        if (nVars < 0) {
            throw ErrGeneric(MBDYN_EXCEPT_ARGS, "genetic_algorithm: variables number must be >= 0");
        }
        
        // Variables will be registered externally via shared memory/pointers
        chromosome_length = nVars;
        
        // Can optionally define variable names and bounds here for documentation
        for (integer i = 0; i < nVars; ++i) {
            if (HP.IsKeyWord("variable")) {
                std::string name = HP.GetString();
                double min_val = -10.0;  // Default bounds
                double max_val = 10.0;
                
                if (HP.IsKeyWord("min")) {
                    min_val = HP.GetReal();
                }
                if (HP.IsKeyWord("max")) {
                    max_val = HP.GetReal();
                }
                
                // Check for shared variable definition
                if (HP.IsKeyWord("shared")) {
                    std::string lib_path = HP.GetString();
                    std::string symbol = HP.GetString();
                    
                    // Register variable from shared library
                    if (!var_registry.registerSharedVariable(lib_path, symbol, name, 
                                                          min_val, max_val)) {
                        silent_cerr("GeneticAlgorithm(" << GetLabel() << "): warning - "
                                  << "failed to register shared variable " << name << std::endl);
                    }
                } else {
                    // Register variable without pointer (will be set later)
                    var_registry.registerVariable(name, nullptr, min_val, max_val);
                }
            }
        }
    }

    if (var_registry.size() > 0) {
        chromosome_length = static_cast<int>(var_registry.size());
    }

    // Parse outputs
    if (HP.IsKeyWord("output" "number")) {
        integer nOut = HP.GetInt();
        if (nOut < 0) {
            throw ErrGeneric(MBDYN_EXCEPT_ARGS, "genetic_algorithm: output number must be >= 0");
        }
        
        // If no variables registered yet, use outputs as chromosome length
        if (var_registry.size() == 0) {
            chromosome_length = nOut;
        }
        
        m_outputs.assign(static_cast<size_t>(nOut), 0.);
        m_outputLabels.resize(static_cast<size_t>(nOut));
        
        for (integer i = 0; i < nOut; ++i) {
            m_outputLabels[static_cast<size_t>(i)] = HP.GetInt();
        }
    }

    // Initialize bounds (default [0, 1] or from variable registry if available)
    if (var_registry.size() > 0) {
        var_registry.getBounds(lower_bounds, upper_bounds);
    } else {
        lower_bounds.assign(chromosome_length, 0.0);
        upper_bounds.assign(chromosome_length, 1.0);
    }
    
    // Apply user-specified bounds if provided (after defaults)
    // Note: population_upper/population_lower parsed later will override these

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
    gaCtxCreate = (GACtxCreateFunc)dlsym(gaLibHandle, "ga_ctx_create");
    gaCtxDestroy = (GACtxDestroyFunc)dlsym(gaLibHandle, "ga_ctx_destroy");
    gaRandomizePopulation = (GARandomizePopulationFunc)dlsym(gaLibHandle, "ga_randomize_population");
    gaRegisterEnvPtrs = (GARegisterEnvPtrsFunc)dlsym(gaLibHandle, "ga_register_env_ptrs");
    gaEvaluatePopulation = (GAEvaluatePopulationFunc)dlsym(gaLibHandle, "ga_evaluate_population");
    gaBestIndex = (GABestIndexFunc)dlsym(gaLibHandle, "ga_best_index");
    gaGetBestIndividual = (GAGetBestIndividualFunc)dlsym(gaLibHandle, "ga_get_best_individual");
    gaGetTopN = (GAGetTopNFunc)dlsym(gaLibHandle, "ga_get_top_n");

    if (!gaCtxCreate) {
        throw ErrGeneric(MBDYN_EXCEPT_ARGS, "GA context creation symbol missing: ga_ctx_create");
    }
    
    if (!gaGetTopN) {
        silent_cerr("GeneticAlgorithm(" << GetLabel() << "): warning - ga_get_top_n() not available" << std::endl);
    }

    // Resolve libcons.so symbols (fitness only)
    if (consLibHandle) {
        evaluateFitness = (EvaluateFitnessFunc)dlsym(consLibHandle, "evaluate_fitness");
        if (!evaluateFitness) {
            silent_cerr("GeneticAlgorithm(" << GetLabel() << "): warning - fitness function not found" << std::endl);
        }
    }

    // Initialize population via libga.so (ONLY initialization)
    unsigned int seed = static_cast<unsigned int>(std::random_device{}());
    gaCtx = gaCtxCreate(population_size, chromosome_length, 
                        lower_bounds.data(), upper_bounds.data(), seed);
    
    if (!gaCtx) {
        throw ErrGeneric(MBDYN_EXCEPT_ARGS, "Failed to create GA context");
    }
    
    // Randomize initial population via libga
    if (gaRandomizePopulation) {
        gaRandomizePopulation(gaCtx, 0.3);  // sigma_fraction = 0.3
    }
    
    // Register environment pointers for fitness evaluation
    if (gaRegisterEnvPtrs) {
        gaRegisterEnvPtrs(gaCtx, m_inputVals.data(), static_cast<int>(m_inputVals.size()),
                          m_outputs.data(), static_cast<int>(m_outputs.size()), nullptr);
    }
    
    // Copy population to MODULE storage (MODULE manages it from now on)
    population.resize(population_size);
    for (int i = 0; i < population_size; ++i) {
        population[i].resize(chromosome_length);
    }

    // Variable registry seeding no longer needed here (libga handles initialization)

    fitness_values.resize(population_size, 0.0);
    if (best_individual.size() != static_cast<size_t>(chromosome_length)) {
        best_individual.assign(chromosome_length, 0.0);
    }

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
    std::cout << "  Registry variables: " << var_registry.size() << std::endl;
    std::cout << "  libga.so: population initialization ONLY" << std::endl;
    std::cout << "  libcons.so: fitness evaluation ONLY" << std::endl;
    std::cout << "  module: ALL GA operations (selection, crossover, mutation)" << std::endl;
}

// Register a variable for optimization
bool GeneticAlgorithm::registerVariable(
    const std::string& name, double* ptr,
    double min_value, double max_value,
    const std::string& description)
{
    return var_registry.registerVariable(name, ptr, min_value, max_value, description);
}

// Register a variable from a shared library
bool GeneticAlgorithm::registerSharedVariable(
    const std::string& library_path,
    const std::string& symbol_name,
    const std::string& var_name,
    double min_value, double max_value,
    const std::string& description)
{
    return var_registry.registerSharedVariable(library_path, symbol_name, var_name, 
                                             min_value, max_value, description);
}

// Apply best solution to registered variables
void GeneticAlgorithm::applyBestSolution() {
    if (var_registry.size() == 0) {
        return;
    }

    if (best_individual.size() < var_registry.size()) {
        return;
    }

    // Decode best solution into variable registry
    var_registry.decodeFromChromosome(best_individual);
    
    // Apply to actual variables
    var_registry.applyValues();
}

// Destructor
GeneticAlgorithm::~GeneticAlgorithm(void)
{
    if (gaCtxDestroy && gaCtx) {
        gaCtxDestroy(gaCtx);
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
        // Apply individual to variable registry for evaluation
        var_registry.decodeFromChromosome(population[i]);
        
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
            
            // Apply best solution to registered variables
            applyBestSolution();
            
            // Update outputs (for backward compatibility)
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
    if (!bToBeOutput()) {
        return;
    }

    std::ostream& out = OH.Loadable();

    out << "Generation " << current_generation << ", Best fitness = " << best_fitness << '\n';
    out << "Best variable values:";
    if (best_individual.empty()) {
        out << " (none)";
    }
    out << '\n';

    for (size_t i = 0; i < best_individual.size(); ++i) {
        std::string varName = var_registry.getNameByIndex(i);
        if (!varName.empty()) {
            out << "  " << varName << " = " << best_individual[i] << '\n';
        } else {
            out << "  var[" << i << "] = " << best_individual[i] << '\n';
        }
    }
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
    
    // Call libga to evaluate population and get top N
    if (gaEvaluatePopulation && gaCtx) {
        gaEvaluatePopulation(gaCtx);
    }
    
    // Get best N individuals and update outputs
    if (gaGetTopN && gaCtx && return_best_population > 0) {
        int N = std::min(return_best_population, static_cast<int>(m_outputs.size() / chromosome_length));
        if (N > 0) {
            std::vector<double> top_genes(static_cast<size_t>(N * chromosome_length));
            int got = gaGetTopN(gaCtx, N, top_genes.data());
            
            // Map top individuals to outputs
            for (int i = 0; i < got && i < N; ++i) {
                for (int j = 0; j < chromosome_length; ++j) {
                    size_t out_idx = static_cast<size_t>(i * chromosome_length + j);
                    if (out_idx < m_outputs.size()) {
                        m_outputs[out_idx] = top_genes[static_cast<size_t>(i * chromosome_length + j)];
                    }
                }
            }
        }
    }
    
    // Always ensure best solution is applied to registered variables
    applyBestSolution();
    
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
    unsigned int outIdx = (i >= 2U) ? (i - 2U) : std::numeric_limits<unsigned int>::max();
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
    
    return 0;
}
