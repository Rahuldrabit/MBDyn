#ifndef CROSSOVER_H
#define CROSSOVER_H

#include <vector>
#include <random>
#include <utility>
#include <functional>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <memory>

// ============================================================================
// LOGGING SYSTEM
// ============================================================================

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

class CrossoverLogger {
private:
    static std::unique_ptr<CrossoverLogger> instance;
    std::ofstream log_file;
    LogLevel min_log_level;
    bool console_output;
    std::string log_filename;
    size_t max_file_size;
    size_t current_file_size;
    
    CrossoverLogger();
    std::string getCurrentTimestamp();
    std::string logLevelToString(LogLevel level);
    void rotateLogFile();
    
public:
    static CrossoverLogger& getInstance();
    
    void initialize(const std::string& filename = "crossover.log", 
                   LogLevel level = LogLevel::INFO, 
                   bool console = true,
                   size_t max_size = 10 * 1024 * 1024); // 10MB default
    
    void log(LogLevel level, const std::string& message, 
             const std::string& function = "", int line = 0);
    
    void logError(const std::string& message, const std::string& function = "", int line = 0);
    void logWarning(const std::string& message, const std::string& function = "", int line = 0);
    void logInfo(const std::string& message, const std::string& function = "", int line = 0);
    void logDebug(const std::string& message, const std::string& function = "", int line = 0);
    
    void setLogLevel(LogLevel level) { min_log_level = level; }
    void enableConsoleOutput(bool enable) { console_output = enable; }
    
    ~CrossoverLogger();
};

// Logging macros for convenience
#define LOG_ERROR(msg) CrossoverLogger::getInstance().logError(msg, __FUNCTION__, __LINE__)
#define LOG_WARNING(msg) CrossoverLogger::getInstance().logWarning(msg, __FUNCTION__, __LINE__)
#define LOG_INFO(msg) CrossoverLogger::getInstance().logInfo(msg, __FUNCTION__, __LINE__)
#define LOG_DEBUG(msg) CrossoverLogger::getInstance().logDebug(msg, __FUNCTION__, __LINE__)

// Forward declarations
class CrossoverOperator;

// Type definitions
using BitString = std::vector<bool>;
using RealVector = std::vector<double>;
using IntVector = std::vector<int>;
using Permutation = std::vector<int>;

// Tree node structure for GP
struct TreeNode {
    std::string value;
    std::vector<TreeNode*> children;
    
    TreeNode(const std::string& val) : value(val) {}
    ~TreeNode();
    TreeNode* clone() const;
};

// Neural network weight structure
struct NeuralNetwork {
    std::vector<std::vector<double>> weights;  // weights[layer][neuron]
    std::vector<std::vector<bool>> connections; // connectivity matrix
};

// Rule structure for rule-based systems
struct Rule {
    std::vector<int> conditions;
    std::vector<int> actions;
    double fitness;
    
    Rule() : fitness(0.0) {}
};

using RuleSet = std::vector<Rule>;

// Base crossover operator class
class CrossoverOperator {
protected:
    std::mt19937 rng;
    std::string operator_name;
    mutable size_t operation_count;
    mutable size_t error_count;
    
    void logOperation(const std::string& operation, bool success = true) const;
    void logError(const std::string& error_msg) const;
    
public:
    CrossoverOperator(const std::string& name = "CrossoverOperator", unsigned seed = std::random_device{}()) 
        : rng(seed), operator_name(name), operation_count(0), error_count(0) {}
    virtual ~CrossoverOperator() = default;
    
    // Statistics methods
    size_t getOperationCount() const { return operation_count; }
    size_t getErrorCount() const { return error_count; }
    double getErrorRate() const { 
        return operation_count > 0 ? static_cast<double>(error_count) / operation_count : 0.0; 
    }
    void resetStatistics() { operation_count = 0; error_count = 0; }
};

// ============================================================================
// BIT-STRING/VECTOR ENCODINGS
// ============================================================================

class OnePointCrossover : public CrossoverOperator {
public:
    OnePointCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("OnePointCrossover", seed) {}
    
    std::pair<BitString, BitString> crossover(const BitString& parent1, const BitString& parent2);
    std::pair<RealVector, RealVector> crossover(const RealVector& parent1, const RealVector& parent2);
    std::pair<IntVector, IntVector> crossover(const IntVector& parent1, const IntVector& parent2);
};

class TwoPointCrossover : public CrossoverOperator {
public:
    TwoPointCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("TwoPointCrossover", seed) {}
    
    std::pair<BitString, BitString> crossover(const BitString& parent1, const BitString& parent2);
    std::pair<RealVector, RealVector> crossover(const RealVector& parent1, const RealVector& parent2);
    std::pair<IntVector, IntVector> crossover(const IntVector& parent1, const IntVector& parent2);
};

class MultiPointCrossover : public CrossoverOperator {
private:
    int num_points;
    
public:
    MultiPointCrossover(int points = 3, unsigned seed = std::random_device{}()) 
        : CrossoverOperator("MultiPointCrossover", seed), num_points(points) {}
    
    std::pair<BitString, BitString> crossover(const BitString& parent1, const BitString& parent2);
    std::pair<RealVector, RealVector> crossover(const RealVector& parent1, const RealVector& parent2);
    std::pair<IntVector, IntVector> crossover(const IntVector& parent1, const IntVector& parent2);
};

class UniformCrossover : public CrossoverOperator {
private:
    double probability;
    
public:
    UniformCrossover(double p = 0.5, unsigned seed = std::random_device{}()) 
        : CrossoverOperator("UniformCrossover", seed), probability(p) {}
    
    std::pair<BitString, BitString> crossover(const BitString& parent1, const BitString& parent2);
    std::pair<RealVector, RealVector> crossover(const RealVector& parent1, const RealVector& parent2);
    std::pair<IntVector, IntVector> crossover(const IntVector& parent1, const IntVector& parent2);
};

class UniformKVectorCrossover : public CrossoverOperator {
private:
    double swap_probability;
    
public:
    UniformKVectorCrossover(double p = 0.1, unsigned seed = std::random_device{}()) 
        : CrossoverOperator("UniformKVectorCrossover", seed), swap_probability(p) {}
    
    std::vector<BitString> crossover(const std::vector<BitString>& parents);
    std::vector<RealVector> crossover(const std::vector<RealVector>& parents);
    std::vector<IntVector> crossover(const std::vector<IntVector>& parents);
};

// ============================================================================
// REAL-VALUED/FLOATING-POINT ENCODINGS
// ============================================================================

class LineRecombination : public CrossoverOperator {
private:
    double extension_factor;
    
public:
    LineRecombination(double p = 0.1, unsigned seed = std::random_device{}()) 
        : CrossoverOperator("LineRecombination", seed), extension_factor(p) {}
    
    std::pair<RealVector, RealVector> crossover(const RealVector& parent1, const RealVector& parent2);
};

class IntermediateRecombination : public CrossoverOperator {
private:
    double alpha;
    
public:
    IntermediateRecombination(double a = 0.5, unsigned seed = std::random_device{}()) 
        : CrossoverOperator("IntermediateRecombination", seed), alpha(a) {}
    
    std::pair<RealVector, RealVector> crossover(const RealVector& parent1, const RealVector& parent2);
    RealVector singleArithmeticRecombination(const RealVector& parent1, const RealVector& parent2);
    RealVector wholeArithmeticRecombination(const RealVector& parent1, const RealVector& parent2);
};

class BlendCrossover : public CrossoverOperator {
private:
    double alpha;
    
public:
    BlendCrossover(double a = 0.5, unsigned seed = std::random_device{}()) 
        : CrossoverOperator("BlendCrossover", seed), alpha(a) {}
    
    std::pair<RealVector, RealVector> crossover(const RealVector& parent1, const RealVector& parent2);
};

class SimulatedBinaryCrossover : public CrossoverOperator {
private:
    double eta_c;  // distribution index
    
public:
    SimulatedBinaryCrossover(double eta = 2.0, unsigned seed = std::random_device{}()) 
        : CrossoverOperator("SimulatedBinaryCrossover", seed), eta_c(eta) {}
    
    std::pair<RealVector, RealVector> crossover(const RealVector& parent1, const RealVector& parent2);
};

// ============================================================================
// PERMUTATION ENCODINGS
// ============================================================================

class CutAndCrossfillCrossover : public CrossoverOperator {
public:
    CutAndCrossfillCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("CutAndCrossfillCrossover", seed) {}
    
    std::pair<Permutation, Permutation> crossover(const Permutation& parent1, const Permutation& parent2);
};

class PartiallyMappedCrossover : public CrossoverOperator {
public:
    PartiallyMappedCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("PartiallyMappedCrossover", seed) {}
    
    std::pair<Permutation, Permutation> crossover(const Permutation& parent1, const Permutation& parent2);
};

class EdgeCrossover : public CrossoverOperator {
public:
    EdgeCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("EdgeCrossover", seed) {}
    
    Permutation crossover(const Permutation& parent1, const Permutation& parent2);
    
private:
    std::map<int, std::set<int>> buildEdgeTable(const Permutation& parent1, const Permutation& parent2);
};

class OrderCrossover : public CrossoverOperator {
public:
    OrderCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("OrderCrossover", seed) {}
    
    std::pair<Permutation, Permutation> crossover(const Permutation& parent1, const Permutation& parent2);
};

class CycleCrossover : public CrossoverOperator {
public:
    CycleCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("CycleCrossover", seed) {}
    
    std::pair<Permutation, Permutation> crossover(const Permutation& parent1, const Permutation& parent2);
    
private:
    std::vector<std::vector<int>> findCycles(const Permutation& parent1, const Permutation& parent2);
};

// ============================================================================
// TREE ENCODINGS (GENETIC PROGRAMMING)
// ============================================================================

class SubtreeCrossover : public CrossoverOperator {
public:
    SubtreeCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("SubtreeCrossover", seed) {}
    
    std::pair<TreeNode*, TreeNode*> crossover(const TreeNode* parent1, const TreeNode* parent2);
    
private:
    TreeNode* selectRandomNode(TreeNode* tree);
    int countNodes(const TreeNode* tree);
    TreeNode* getNodeAtIndex(TreeNode* tree, int index, int& current);
};

// ============================================================================
// SPECIALIZED CROSSOVER TYPES
// ============================================================================

class DiploidRecombination : public CrossoverOperator {
public:
    DiploidRecombination(unsigned seed = std::random_device{}()) : CrossoverOperator("DiploidRecombination", seed) {}
    
    using DiploidChromosome = std::pair<BitString, BitString>;
    DiploidChromosome crossover(const DiploidChromosome& parent1, const DiploidChromosome& parent2);
    
private:
    BitString formGamete(const DiploidChromosome& parent);
};

class CrossoverTemplate : public CrossoverOperator {
public:
    CrossoverTemplate(unsigned seed = std::random_device{}()) : CrossoverOperator("CrossoverTemplate", seed) {}
    
    struct TemplatedChromosome {
        BitString chromosome;
        BitString template_bits;
    };
    
    std::pair<TemplatedChromosome, TemplatedChromosome> crossover(
        const TemplatedChromosome& parent1, const TemplatedChromosome& parent2);
};

class NeuralNetworkWeightCrossover : public CrossoverOperator {
public:
    NeuralNetworkWeightCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("NeuralNetworkWeightCrossover", seed) {}
    
    NeuralNetwork crossover(const NeuralNetwork& parent1, const NeuralNetwork& parent2);
};

class NeuralNetworkTopologyCrossover : public CrossoverOperator {
public:
    NeuralNetworkTopologyCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("NeuralNetworkTopologyCrossover", seed) {}
    
    std::pair<NeuralNetwork, NeuralNetwork> crossover(const NeuralNetwork& parent1, const NeuralNetwork& parent2);
};

class RulesetCrossover : public CrossoverOperator {
public:
    RulesetCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("RulesetCrossover", seed) {}
    
    std::pair<RuleSet, RuleSet> uniformCrossover(const RuleSet& parent1, const RuleSet& parent2);
    std::pair<RuleSet, RuleSet> variableLengthCrossover(const RuleSet& parent1, const RuleSet& parent2);
};

class ClusteredCrossover : public CrossoverOperator {
private:
    std::map<std::pair<int, int>, double> rule_clusters; // co-adaptation scores
    
public:
    ClusteredCrossover(unsigned seed = std::random_device{}()) : CrossoverOperator("ClusteredCrossover", seed) {}
    
    void updateClusterInfo(const std::vector<std::pair<int, int>>& rule_pairs, 
                          const std::vector<double>& performance_scores);
    std::pair<RuleSet, RuleSet> crossover(const RuleSet& parent1, const RuleSet& parent2);
};

class Segregation : public CrossoverOperator {
public:
    Segregation(unsigned seed = std::random_device{}()) : CrossoverOperator("Segregation", seed) {}
    
    using MultiChromosome = std::vector<BitString>;
    BitString formGamete(const MultiChromosome& parent);
    MultiChromosome crossover(const MultiChromosome& parent1, const MultiChromosome& parent2);
};

class IntelligentCrossover : public CrossoverOperator {
private:
    std::function<double(const BitString&)> fitness_function;
    
public:
    IntelligentCrossover(std::function<double(const BitString&)> fitness_fn, 
                        unsigned seed = std::random_device{}()) 
        : CrossoverOperator("IntelligentCrossover", seed), fitness_function(fitness_fn) {}
    
    std::pair<BitString, BitString> crossover(const BitString& parent1, const BitString& parent2);
};

class DistancePreservingCrossover : public CrossoverOperator {
private:
    std::vector<std::vector<double>> distance_matrix;
    
public:
    DistancePreservingCrossover(const std::vector<std::vector<double>>& distances, 
                               unsigned seed = std::random_device{}()) 
        : CrossoverOperator("DistancePreservingCrossover", seed), distance_matrix(distances) {}
    
    Permutation crossover(const Permutation& parent1, const Permutation& parent2);
    
private:
    std::set<std::pair<int, int>> getCommonEdges(const Permutation& parent1, const Permutation& parent2);
    int findNearestUnvisited(int current, const std::set<int>& unvisited);
};

// ============================================================================
// EVOLUTION STRATEGIES RECOMBINATION
// ============================================================================

class DiscreteRecombination : public CrossoverOperator {
public:
    DiscreteRecombination(unsigned seed = std::random_device{}()) : CrossoverOperator("DiscreteRecombination", seed) {}
    
    RealVector crossover(const RealVector& parent1, const RealVector& parent2);
};

class GlobalRecombination : public CrossoverOperator {
public:
    GlobalRecombination(unsigned seed = std::random_device{}()) : CrossoverOperator("GlobalRecombination", seed) {}
    
    RealVector crossover(const std::vector<RealVector>& population);
};

// ============================================================================
// DIFFERENTIAL EVOLUTION CROSSOVER
// ============================================================================

class DifferentialEvolutionCrossover : public CrossoverOperator {
private:
    double CR; // crossover rate
    
public:
    DifferentialEvolutionCrossover(double crossover_rate = 0.9, unsigned seed = std::random_device{}()) 
        : CrossoverOperator("DifferentialEvolutionCrossover", seed), CR(crossover_rate) {}
    
    RealVector crossover(const RealVector& target, const RealVector& mutant);
};

#endif // CROSSOVER_H