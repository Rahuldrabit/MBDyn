#include "crossover.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <queue>
#include <stdexcept>
#include <iostream>
#include <climits>
#include <cstdio>  // for std::rename

// ============================================================================
// LOGGING SYSTEM IMPLEMENTATION
// ============================================================================

std::unique_ptr<CrossoverLogger> CrossoverLogger::instance = nullptr;

CrossoverLogger::CrossoverLogger() 
    : min_log_level(LogLevel::INFO), console_output(true), max_file_size(10 * 1024 * 1024), current_file_size(0) {
}

CrossoverLogger& CrossoverLogger::getInstance() {
    if (!instance) {
        instance = std::unique_ptr<CrossoverLogger>(new CrossoverLogger());
    }
    return *instance;
}

void CrossoverLogger::initialize(const std::string& filename, LogLevel level, bool console, size_t max_size) {
    log_filename = filename;
    min_log_level = level;
    console_output = console;
    max_file_size = max_size;
    
    if (log_file.is_open()) {
        log_file.close();
    }
    
    log_file.open(log_filename, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Failed to open log file: " << log_filename << std::endl;
    } else {
        // Get current file size
        log_file.seekp(0, std::ios::end);
        current_file_size = log_file.tellp();
        log(LogLevel::INFO, "Logging system initialized", "CrossoverLogger", __LINE__);
    }
}

std::string CrossoverLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string CrossoverLogger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void CrossoverLogger::rotateLogFile() {
    if (log_file.is_open()) {
        log_file.close();
    }
    
    // Rename current log file
    std::string backup_name = log_filename + ".bak";
    std::rename(log_filename.c_str(), backup_name.c_str());
    
    // Open new log file
    log_file.open(log_filename, std::ios::app);
    current_file_size = 0;
    
    if (log_file.is_open()) {
        log(LogLevel::INFO, "Log file rotated", "rotateLogFile", __LINE__);
    }
}

void CrossoverLogger::log(LogLevel level, const std::string& message, const std::string& function, int line) {
    if (level < min_log_level) return;
    
    std::stringstream log_entry;
    log_entry << "[" << getCurrentTimestamp() << "] "
              << "[" << logLevelToString(level) << "] ";
    
    if (!function.empty()) {
        log_entry << "[" << function;
        if (line > 0) {
            log_entry << ":" << line;
        }
        log_entry << "] ";
    }
    
    log_entry << message;
    
    std::string full_message = log_entry.str();
    
    // Console output
    if (console_output) {
        if (level >= LogLevel::ERROR) {
            std::cerr << full_message << std::endl;
        } else {
            std::cout << full_message << std::endl;
        }
    }
    
    // File output
    if (log_file.is_open()) {
        // Check if rotation is needed
        if (current_file_size + full_message.length() > max_file_size) {
            rotateLogFile();
        }
        
        log_file << full_message << std::endl;
        log_file.flush();
        current_file_size += full_message.length() + 1; // +1 for newline
    }
}

void CrossoverLogger::logError(const std::string& message, const std::string& function, int line) {
    log(LogLevel::ERROR, message, function, line);
}

void CrossoverLogger::logWarning(const std::string& message, const std::string& function, int line) {
    log(LogLevel::WARNING, message, function, line);
}

void CrossoverLogger::logInfo(const std::string& message, const std::string& function, int line) {
    log(LogLevel::INFO, message, function, line);
}

void CrossoverLogger::logDebug(const std::string& message, const std::string& function, int line) {
    log(LogLevel::DEBUG, message, function, line);
}

CrossoverLogger::~CrossoverLogger() {
    if (log_file.is_open()) {
        log(LogLevel::INFO, "Logging system shutting down", "~CrossoverLogger", __LINE__);
        log_file.close();
    }
}

// ============================================================================
// BASE CROSSOVER OPERATOR IMPLEMENTATION
// ============================================================================

void CrossoverOperator::logOperation(const std::string& operation, bool success) const {
    const_cast<size_t&>(operation_count)++;
    if (!success) {
        const_cast<size_t&>(error_count)++;
    }
    
    std::stringstream msg;
    msg << operator_name << ": " << operation << " - " << (success ? "SUCCESS" : "FAILED")
        << " (Operations: " << operation_count << ", Errors: " << error_count << ")";
    
    if (success) {
        LOG_DEBUG(msg.str());
    } else {
        LOG_ERROR(msg.str());
    }
}

void CrossoverOperator::logError(const std::string& error_msg) const {
    const_cast<size_t&>(error_count)++;
    std::stringstream msg;
    msg << operator_name << " ERROR: " << error_msg;
    LOG_ERROR(msg.str());
}

// ============================================================================
// TREE NODE IMPLEMENTATION
// ============================================================================

TreeNode::~TreeNode() {
    for (auto* child : children) {
        delete child;
    }
}

TreeNode* TreeNode::clone() const {
    TreeNode* copy = new TreeNode(value);
    for (const auto* child : children) {
        copy->children.push_back(child->clone());
    }
    return copy;
}

// ============================================================================
// ONE-POINT CROSSOVER
// ============================================================================

std::pair<BitString, BitString> OnePointCrossover::crossover(const BitString& parent1, const BitString& parent2) {
    try {
        LOG_DEBUG("OnePointCrossover: Starting crossover operation");
        
        if (parent1.size() != parent2.size()) {
            logError("Parents must have the same length");
            throw std::invalid_argument("Parents must have the same length");
        }
        
        size_t length = parent1.size();
        if (length <= 1) {
            LOG_WARNING("OnePointCrossover: Length <= 1, returning original parents");
            logOperation("crossover (BitString)", true);
            return {parent1, parent2};
        }
        
        std::uniform_int_distribution<size_t> dist(1, length - 1);
        size_t crossover_point = dist(rng);
        
        LOG_DEBUG("OnePointCrossover: Crossover point selected at position " + std::to_string(crossover_point));
        
        BitString child1 = parent1;
        BitString child2 = parent2;
        
        for (size_t i = crossover_point; i < length; ++i) {
            child1[i] = parent2[i];
            child2[i] = parent1[i];
        }
        
        logOperation("crossover (BitString)", true);
        LOG_DEBUG("OnePointCrossover: Crossover completed successfully");
        return {child1, child2};
        
    } catch (const std::exception& e) {
        logError("Exception in crossover (BitString): " + std::string(e.what()));
        logOperation("crossover (BitString)", false);
        throw;
    }
}

std::pair<RealVector, RealVector> OnePointCrossover::crossover(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    if (length <= 1) return {parent1, parent2};
    
    std::uniform_int_distribution<size_t> dist(1, length - 1);
    size_t crossover_point = dist(rng);
    
    RealVector child1 = parent1;
    RealVector child2 = parent2;
    
    for (size_t i = crossover_point; i < length; ++i) {
        child1[i] = parent2[i];
        child2[i] = parent1[i];
    }
    
    return {child1, child2};
}

std::pair<IntVector, IntVector> OnePointCrossover::crossover(const IntVector& parent1, const IntVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    if (length <= 1) return {parent1, parent2};
    
    std::uniform_int_distribution<size_t> dist(1, length - 1);
    size_t crossover_point = dist(rng);
    
    IntVector child1 = parent1;
    IntVector child2 = parent2;
    
    for (size_t i = crossover_point; i < length; ++i) {
        child1[i] = parent2[i];
        child2[i] = parent1[i];
    }
    
    return {child1, child2};
}

// ============================================================================
// TWO-POINT CROSSOVER
// ============================================================================

std::pair<BitString, BitString> TwoPointCrossover::crossover(const BitString& parent1, const BitString& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    if (length <= 2) return {parent1, parent2};
    
    std::uniform_int_distribution<size_t> dist(0, length - 1);
    size_t point1 = dist(rng);
    size_t point2 = dist(rng);
    
    if (point1 > point2) std::swap(point1, point2);
    if (point1 == point2) {
        if (point2 < length - 1) point2++;
        else point1--;
    }
    
    BitString child1 = parent1;
    BitString child2 = parent2;
    
    for (size_t i = point1; i <= point2; ++i) {
        child1[i] = parent2[i];
        child2[i] = parent1[i];
    }
    
    return {child1, child2};
}

std::pair<RealVector, RealVector> TwoPointCrossover::crossover(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    if (length <= 2) return {parent1, parent2};
    
    std::uniform_int_distribution<size_t> dist(0, length - 1);
    size_t point1 = dist(rng);
    size_t point2 = dist(rng);
    
    if (point1 > point2) std::swap(point1, point2);
    if (point1 == point2) {
        if (point2 < length - 1) point2++;
        else point1--;
    }
    
    RealVector child1 = parent1;
    RealVector child2 = parent2;
    
    for (size_t i = point1; i <= point2; ++i) {
        child1[i] = parent2[i];
        child2[i] = parent1[i];
    }
    
    return {child1, child2};
}

std::pair<IntVector, IntVector> TwoPointCrossover::crossover(const IntVector& parent1, const IntVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    if (length <= 2) return {parent1, parent2};
    
    std::uniform_int_distribution<size_t> dist(0, length - 1);
    size_t point1 = dist(rng);
    size_t point2 = dist(rng);
    
    if (point1 > point2) std::swap(point1, point2);
    if (point1 == point2) {
        if (point2 < length - 1) point2++;
        else point1--;
    }
    
    IntVector child1 = parent1;
    IntVector child2 = parent2;
    
    for (size_t i = point1; i <= point2; ++i) {
        child1[i] = parent2[i];
        child2[i] = parent1[i];
    }
    
    return {child1, child2};
}

// ============================================================================
// MULTI-POINT CROSSOVER
// ============================================================================

std::pair<BitString, BitString> MultiPointCrossover::crossover(const BitString& parent1, const BitString& parent2) {
    try {
        LOG_DEBUG("MultiPointCrossover: Starting crossover with " + std::to_string(num_points) + " points");
        
        if (parent1.size() != parent2.size()) {
            logError("Parents must have the same length");
            throw std::invalid_argument("Parents must have the same length");
        }
        
        size_t length = parent1.size();
        int actual_points = std::min(num_points, static_cast<int>(length - 1));
        if (actual_points <= 0) {
            LOG_WARNING("MultiPointCrossover: No crossover points possible, returning original parents");
            logOperation("crossover (BitString)", true);
            return {parent1, parent2};
        }
        
        std::vector<size_t> points;
        std::uniform_int_distribution<size_t> dist(1, length - 1);
        
        std::set<size_t> unique_points;  // Use set instead of unordered_set for deterministic ordering
        int attempts = 0;
        const int max_attempts = length * 2; // Prevent infinite loop
        
        while (unique_points.size() < static_cast<size_t>(actual_points) && attempts < max_attempts) {
            unique_points.insert(dist(rng));
            attempts++;
        }
        
        if (attempts >= max_attempts) {
            LOG_WARNING("MultiPointCrossover: Maximum attempts reached for point selection");
        }
        
        points.assign(unique_points.begin(), unique_points.end());
        // points are already sorted since we used std::set
        
        LOG_DEBUG("MultiPointCrossover: Selected " + std::to_string(points.size()) + " crossover points");
        
        BitString child1 = parent1;
        BitString child2 = parent2;
        
        bool swap = false;
        size_t current_point = 0;
        
        for (size_t i = 0; i < length; ++i) {
            if (current_point < points.size() && i == points[current_point]) {
                swap = !swap;
                current_point++;
            }
            
            if (swap) {
                child1[i] = parent2[i];
                child2[i] = parent1[i];
            }
        }
        
        logOperation("crossover (BitString)", true);
        LOG_DEBUG("MultiPointCrossover: Crossover completed successfully");
        return {child1, child2};
        
    } catch (const std::exception& e) {
        logError("Exception in crossover (BitString): " + std::string(e.what()));
        logOperation("crossover (BitString)", false);
        throw;
    }
}

std::pair<RealVector, RealVector> MultiPointCrossover::crossover(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    int actual_points = std::min(num_points, static_cast<int>(length - 1));
    if (actual_points <= 0) return {parent1, parent2};
    
    std::vector<size_t> points;
    std::uniform_int_distribution<size_t> dist(1, length - 1);
    
    std::set<size_t> unique_points;  // Use set instead of unordered_set
    int attempts = 0;
    const int max_attempts = length * 2;
    
    while (unique_points.size() < static_cast<size_t>(actual_points) && attempts < max_attempts) {
        unique_points.insert(dist(rng));
        attempts++;
    }
    
    points.assign(unique_points.begin(), unique_points.end());
    
    RealVector child1 = parent1;
    RealVector child2 = parent2;
    
    bool swap = false;
    size_t current_point = 0;
    
    for (size_t i = 0; i < length; ++i) {
        if (current_point < points.size() && i == points[current_point]) {
            swap = !swap;
            current_point++;
        }
        
        if (swap) {
            child1[i] = parent2[i];
            child2[i] = parent1[i];
        }
    }
    
    return {child1, child2};
}

std::pair<IntVector, IntVector> MultiPointCrossover::crossover(const IntVector& parent1, const IntVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    int actual_points = std::min(num_points, static_cast<int>(length - 1));
    if (actual_points <= 0) return {parent1, parent2};
    
    std::vector<size_t> points;
    std::uniform_int_distribution<size_t> dist(1, length - 1);
    
    std::set<size_t> unique_points;  // Use set instead of unordered_set
    int attempts = 0;
    const int max_attempts = length * 2;
    
    while (unique_points.size() < static_cast<size_t>(actual_points) && attempts < max_attempts) {
        unique_points.insert(dist(rng));
        attempts++;
    }
    
    points.assign(unique_points.begin(), unique_points.end());
    
    IntVector child1 = parent1;
    IntVector child2 = parent2;
    
    bool swap = false;
    size_t current_point = 0;
    
    for (size_t i = 0; i < length; ++i) {
        if (current_point < points.size() && i == points[current_point]) {
            swap = !swap;
            current_point++;
        }
        
        if (swap) {
            child1[i] = parent2[i];
            child2[i] = parent1[i];
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// LINE RECOMBINATION
// ============================================================================

std::pair<RealVector, RealVector> LineRecombination::crossover(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    std::uniform_real_distribution<double> dist(-extension_factor, 1.0 + extension_factor);
    
    RealVector child1, child2;
    child1.reserve(parent1.size());
    child2.reserve(parent1.size());
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        double alpha1 = dist(rng);
        double alpha2 = dist(rng);
        
        child1.push_back(alpha1 * parent1[i] + (1.0 - alpha1) * parent2[i]);
        child2.push_back(alpha2 * parent1[i] + (1.0 - alpha2) * parent2[i]);
    }
    
    return {child1, child2};
}

// ============================================================================
// INTERMEDIATE RECOMBINATION
// ============================================================================

std::pair<RealVector, RealVector> IntermediateRecombination::crossover(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    RealVector child1, child2;
    child1.reserve(parent1.size());
    child2.reserve(parent1.size());
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        child1.push_back(alpha * parent1[i] + (1.0 - alpha) * parent2[i]);
        child2.push_back(alpha * parent2[i] + (1.0 - alpha) * parent1[i]);
    }
    
    return {child1, child2};
}

RealVector IntermediateRecombination::singleArithmeticRecombination(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    std::uniform_int_distribution<size_t> dist(0, parent1.size() - 1);
    size_t position = dist(rng);
    
    RealVector child = parent1;
    child[position] = alpha * parent1[position] + (1.0 - alpha) * parent2[position];
    
    return child;
}

RealVector IntermediateRecombination::wholeArithmeticRecombination(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    RealVector child;
    child.reserve(parent1.size());
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        child.push_back(alpha * parent1[i] + (1.0 - alpha) * parent2[i]);
    }
    
    return child;
}

// ============================================================================
// BLEND CROSSOVER (BLX-α)
// ============================================================================

std::pair<RealVector, RealVector> BlendCrossover::crossover(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    RealVector child1, child2;
    child1.reserve(parent1.size());
    child2.reserve(parent1.size());
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        double min_val = std::min(parent1[i], parent2[i]);
        double max_val = std::max(parent1[i], parent2[i]);
        double range = max_val - min_val;
        
        double lower_bound = min_val - alpha * range;
        double upper_bound = max_val + alpha * range;
        
        std::uniform_real_distribution<double> dist(lower_bound, upper_bound);
        
        child1.push_back(dist(rng));
        child2.push_back(dist(rng));
    }
    
    return {child1, child2};
}

// ============================================================================
// SIMULATED BINARY CROSSOVER (SBX)
// ============================================================================

std::pair<RealVector, RealVector> SimulatedBinaryCrossover::crossover(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    RealVector child1 = parent1;
    RealVector child2 = parent2;
    
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        if (std::abs(parent1[i] - parent2[i]) > 1e-14) {
            double u = dist(rng);
            double beta;
            
            if (u <= 0.5) {
                beta = std::pow(2.0 * u, 1.0 / (eta_c + 1.0));
            } else {
                beta = std::pow(1.0 / (2.0 * (1.0 - u)), 1.0 / (eta_c + 1.0));
            }
            
            child1[i] = 0.5 * ((parent1[i] + parent2[i]) - beta * std::abs(parent2[i] - parent1[i]));
            child2[i] = 0.5 * ((parent1[i] + parent2[i]) + beta * std::abs(parent2[i] - parent1[i]));
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// CUT-AND-CROSSFILL CROSSOVER
// ============================================================================

std::pair<Permutation, Permutation> CutAndCrossfillCrossover::crossover(const Permutation& parent1, const Permutation& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    if (length <= 1) return {parent1, parent2};
    
    std::uniform_int_distribution<size_t> dist(1, length - 1);
    size_t cut_point = dist(rng);
    
    Permutation child1, child2;
    child1.reserve(length);
    child2.reserve(length);
    
    // Copy first segments
    for (size_t i = 0; i < cut_point; ++i) {
        child1.push_back(parent1[i]);
        child2.push_back(parent2[i]);
    }
    
    // Fill remaining positions from other parent
    std::unordered_set<int> used1(child1.begin(), child1.end());
    std::unordered_set<int> used2(child2.begin(), child2.end());
    
    for (size_t i = 0; i < length; ++i) {
        if (used1.find(parent2[i]) == used1.end()) {
            child1.push_back(parent2[i]);
            used1.insert(parent2[i]);
        }
        if (used2.find(parent1[i]) == used2.end()) {
            child2.push_back(parent1[i]);
            used2.insert(parent1[i]);
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// PARTIALLY MAPPED CROSSOVER (PMX)
// ============================================================================

std::pair<Permutation, Permutation> PartiallyMappedCrossover::crossover(const Permutation& parent1, const Permutation& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    if (length <= 2) return {parent1, parent2};
    
    std::uniform_int_distribution<size_t> dist(0, length - 1);
    size_t point1 = dist(rng);
    size_t point2 = dist(rng);
    
    if (point1 > point2) std::swap(point1, point2);
    
    Permutation child1(length, -1);
    Permutation child2(length, -1);
    
    // Copy mapping sections
    std::map<int, int> mapping1, mapping2;
    for (size_t i = point1; i <= point2; ++i) {
        child1[i] = parent2[i];
        child2[i] = parent1[i];
        mapping1[parent2[i]] = parent1[i];
        mapping2[parent1[i]] = parent2[i];
    }
    
    // Fill remaining positions
    for (size_t i = 0; i < length; ++i) {
        if (i < point1 || i > point2) {
            // For child1
            int val1 = parent1[i];
            while (mapping1.find(val1) != mapping1.end()) {
                val1 = mapping1[val1];
            }
            child1[i] = val1;
            
            // For child2
            int val2 = parent2[i];
            while (mapping2.find(val2) != mapping2.end()) {
                val2 = mapping2[val2];
            }
            child2[i] = val2;
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// EDGE CROSSOVER
// ============================================================================

std::map<int, std::set<int>> EdgeCrossover::buildEdgeTable(const Permutation& parent1, const Permutation& parent2) {
    std::map<int, std::set<int>> edge_table;
    
    // Add edges from parent1
    for (size_t i = 0; i < parent1.size(); ++i) {
        int current = parent1[i];
        int prev = parent1[(i - 1 + parent1.size()) % parent1.size()];
        int next = parent1[(i + 1) % parent1.size()];
        
        edge_table[current].insert(prev);
        edge_table[current].insert(next);
    }
    
    // Add edges from parent2
    for (size_t i = 0; i < parent2.size(); ++i) {
        int current = parent2[i];
        int prev = parent2[(i - 1 + parent2.size()) % parent2.size()];
        int next = parent2[(i + 1) % parent2.size()];
        
        edge_table[current].insert(prev);
        edge_table[current].insert(next);
    }
    
    return edge_table;
}

Permutation EdgeCrossover::crossover(const Permutation& parent1, const Permutation& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    auto edge_table = buildEdgeTable(parent1, parent2);
    Permutation child;
    child.reserve(parent1.size());
    
    // Start with random city from parent1
    std::uniform_int_distribution<size_t> dist(0, parent1.size() - 1);
    int current = parent1[dist(rng)];
    child.push_back(current);
    
    std::unordered_set<int> visited;
    visited.insert(current);
    
    while (child.size() < parent1.size()) {
        // Remove current city from all edge lists
        for (auto& [city, edges] : edge_table) {
            edges.erase(current);
        }
        
        // Find next city with minimum edge count
        int next_city = -1;
        int min_edges = INT_MAX;
        
        for (int neighbor : edge_table[current]) {
            if (visited.find(neighbor) == visited.end()) {
                int edge_count = edge_table[neighbor].size();
                if (edge_count < min_edges) {
                    min_edges = edge_count;
                    next_city = neighbor;
                }
            }
        }
        
        // If no connected city found, pick random unvisited
        if (next_city == -1) {
            for (const auto& [city, edges] : edge_table) {
                if (visited.find(city) == visited.end()) {
                    next_city = city;
                    break;
                }
            }
        }
        
        current = next_city;
        child.push_back(current);
        visited.insert(current);
    }
    
    return child;
}

// ============================================================================
// ORDER CROSSOVER
// ============================================================================

std::pair<Permutation, Permutation> OrderCrossover::crossover(const Permutation& parent1, const Permutation& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    size_t length = parent1.size();
    if (length <= 2) return {parent1, parent2};
    
    std::uniform_int_distribution<size_t> dist(0, length - 1);
    size_t point1 = dist(rng);
    size_t point2 = dist(rng);
    
    if (point1 > point2) std::swap(point1, point2);
    
    Permutation child1(length, -1);
    Permutation child2(length, -1);
    
    // Copy selected segments
    std::unordered_set<int> used1, used2;
    for (size_t i = point1; i <= point2; ++i) {
        child1[i] = parent1[i];
        child2[i] = parent2[i];
        used1.insert(parent1[i]);
        used2.insert(parent2[i]);
    }
    
    // Fill remaining positions
    auto fillChild = [&](Permutation& child, const Permutation& source, std::unordered_set<int>& used) {
        size_t pos = (point2 + 1) % length;
        for (size_t i = 0; i < length; ++i) {
            size_t source_pos = (point2 + 1 + i) % length;
            if (used.find(source[source_pos]) == used.end()) {
                child[pos] = source[source_pos];
                used.insert(source[source_pos]);
                pos = (pos + 1) % length;
            }
        }
    };
    
    fillChild(child1, parent2, used1);
    fillChild(child2, parent1, used2);
    
    return {child1, child2};
}

// ============================================================================
// CYCLE CROSSOVER
// ============================================================================

std::vector<std::vector<int>> CycleCrossover::findCycles(const Permutation& parent1, const Permutation& parent2) {
    std::vector<std::vector<int>> cycles;
    std::vector<bool> visited(parent1.size(), false);
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        if (!visited[i]) {
            std::vector<int> cycle;
            size_t current = i;
            
            do {
                visited[current] = true;
                cycle.push_back(current);
                
                // Find where parent1[current] appears in parent2
                auto it = std::find(parent2.begin(), parent2.end(), parent1[current]);
                current = std::distance(parent2.begin(), it);
            } while (!visited[current]);
            
            cycles.push_back(cycle);
        }
    }
    
    return cycles;
}

std::pair<Permutation, Permutation> CycleCrossover::crossover(const Permutation& parent1, const Permutation& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    auto cycles = findCycles(parent1, parent2);
    
    Permutation child1 = parent2;
    Permutation child2 = parent1;
    
    // Alternate cycles between parents
    for (size_t i = 0; i < cycles.size(); i += 2) {
        for (int pos : cycles[i]) {
            child1[pos] = parent1[pos];
            child2[pos] = parent2[pos];
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// SUBTREE CROSSOVER
// ============================================================================

int SubtreeCrossover::countNodes(const TreeNode* tree) {
    if (!tree) return 0;
    int count = 1;
    for (const auto* child : tree->children) {
        count += countNodes(child);
    }
    return count;
}

TreeNode* SubtreeCrossover::getNodeAtIndex(TreeNode* tree, int index, int& current) {
    if (!tree) return nullptr;
    if (current == index) return tree;
    current++;
    
    for (auto* child : tree->children) {
        TreeNode* result = getNodeAtIndex(child, index, current);
        if (result) return result;
    }
    return nullptr;
}

TreeNode* SubtreeCrossover::selectRandomNode(TreeNode* tree) {
    int node_count = countNodes(tree);
    std::uniform_int_distribution<int> dist(0, node_count - 1);
    int target_index = dist(rng);
    
    int current = 0;
    return getNodeAtIndex(tree, target_index, current);
}

std::pair<TreeNode*, TreeNode*> SubtreeCrossover::crossover(const TreeNode* parent1, const TreeNode* parent2) {
    if (!parent1 || !parent2) {
        throw std::invalid_argument("Parents cannot be null");
    }
    
    TreeNode* child1 = parent1->clone();
    TreeNode* child2 = parent2->clone();
    
    TreeNode* node1 = selectRandomNode(child1);
    TreeNode* node2 = selectRandomNode(child2);
    
    // Swap the subtrees by swapping their values and children
    std::swap(node1->value, node2->value);
    std::swap(node1->children, node2->children);
    
    return {child1, child2};
}

// ============================================================================
// DIPLOID RECOMBINATION
// ============================================================================

BitString DiploidRecombination::formGamete(const DiploidChromosome& parent) {
    const auto& [chromosome1, chromosome2] = parent;
    if (chromosome1.size() != chromosome2.size()) {
        throw std::invalid_argument("Diploid chromosomes must have same length");
    }
    
    BitString gamete;
    gamete.reserve(chromosome1.size());
    
    std::uniform_int_distribution<int> dist(0, 1);
    
    for (size_t i = 0; i < chromosome1.size(); ++i) {
        if (dist(rng) == 0) {
            gamete.push_back(chromosome1[i]);
        } else {
            gamete.push_back(chromosome2[i]);
        }
    }
    
    return gamete;
}

DiploidRecombination::DiploidChromosome DiploidRecombination::crossover(const DiploidChromosome& parent1, const DiploidChromosome& parent2) {
    BitString gamete1 = formGamete(parent1);
    BitString gamete2 = formGamete(parent2);
    
    return {gamete1, gamete2};
}

// ============================================================================
// CROSSOVER TEMPLATE
// ============================================================================

std::pair<CrossoverTemplate::TemplatedChromosome, CrossoverTemplate::TemplatedChromosome> 
CrossoverTemplate::crossover(const TemplatedChromosome& parent1, const TemplatedChromosome& parent2) {
    if (parent1.chromosome.size() != parent2.chromosome.size() ||
        parent1.template_bits.size() != parent2.template_bits.size() ||
        parent1.chromosome.size() != parent1.template_bits.size()) {
        throw std::invalid_argument("All components must have the same length");
    }
    
    TemplatedChromosome child1, child2;
    child1.chromosome.reserve(parent1.chromosome.size());
    child1.template_bits.reserve(parent1.template_bits.size());
    child2.chromosome.reserve(parent2.chromosome.size());
    child2.template_bits.reserve(parent2.template_bits.size());
    
    std::uniform_int_distribution<int> dist(0, 1);
    
    for (size_t i = 0; i < parent1.chromosome.size(); ++i) {
        // Check if crossover is allowed at this position
        if (parent1.template_bits[i] && parent2.template_bits[i]) {
            // Crossover allowed, swap with probability 0.5
            if (dist(rng) == 0) {
                child1.chromosome.push_back(parent1.chromosome[i]);
                child2.chromosome.push_back(parent2.chromosome[i]);
            } else {
                child1.chromosome.push_back(parent2.chromosome[i]);
                child2.chromosome.push_back(parent1.chromosome[i]);
            }
        } else {
            // No crossover allowed, copy directly
            child1.chromosome.push_back(parent1.chromosome[i]);
            child2.chromosome.push_back(parent2.chromosome[i]);
        }
        
        // Inherit template bits
        if (dist(rng) == 0) {
            child1.template_bits.push_back(parent1.template_bits[i]);
            child2.template_bits.push_back(parent2.template_bits[i]);
        } else {
            child1.template_bits.push_back(parent2.template_bits[i]);
            child2.template_bits.push_back(parent1.template_bits[i]);
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// NEURAL NETWORK WEIGHT CROSSOVER
// ============================================================================

NeuralNetwork NeuralNetworkWeightCrossover::crossover(const NeuralNetwork& parent1, const NeuralNetwork& parent2) {
    if (parent1.weights.size() != parent2.weights.size()) {
        throw std::invalid_argument("Neural networks must have same structure");
    }
    
    NeuralNetwork child;
    child.weights.resize(parent1.weights.size());
    child.connections = parent1.connections; // Copy connection structure
    
    std::uniform_int_distribution<int> dist(0, 1);
    
    for (size_t layer = 0; layer < parent1.weights.size(); ++layer) {
        if (parent1.weights[layer].size() != parent2.weights[layer].size()) {
            throw std::invalid_argument("Layers must have same size");
        }
        
        child.weights[layer].resize(parent1.weights[layer].size());
        
        for (size_t unit = 0; unit < parent1.weights[layer].size(); ++unit) {
            // Randomly choose parent for this unit's weights
            if (dist(rng) == 0) {
                child.weights[layer][unit] = parent1.weights[layer][unit];
            } else {
                child.weights[layer][unit] = parent2.weights[layer][unit];
            }
        }
    }
    
    return child;
}

// ============================================================================
// NEURAL NETWORK TOPOLOGY CROSSOVER
// ============================================================================

std::pair<NeuralNetwork, NeuralNetwork> NeuralNetworkTopologyCrossover::crossover(const NeuralNetwork& parent1, const NeuralNetwork& parent2) {
    if (parent1.connections.size() != parent2.connections.size()) {
        throw std::invalid_argument("Neural networks must have same structure");
    }
    
    NeuralNetwork child1 = parent1;
    NeuralNetwork child2 = parent2;
    
    if (!parent1.connections.empty()) {
        std::uniform_int_distribution<size_t> dist(0, parent1.connections.size() - 1);
        size_t row_to_swap = dist(rng);
        
        std::swap(child1.connections[row_to_swap], child2.connections[row_to_swap]);
        
        // Also swap corresponding weights if they exist
        if (row_to_swap < child1.weights.size() && row_to_swap < child2.weights.size()) {
            std::swap(child1.weights[row_to_swap], child2.weights[row_to_swap]);
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// RULESET CROSSOVER
// ============================================================================

std::pair<RuleSet, RuleSet> RulesetCrossover::uniformCrossover(const RuleSet& parent1, const RuleSet& parent2) {
    size_t min_size = std::min(parent1.size(), parent2.size());
    
    RuleSet child1, child2;
    child1.reserve(min_size);
    child2.reserve(min_size);
    
    std::uniform_int_distribution<int> dist(0, 1);
    
    for (size_t i = 0; i < min_size; ++i) {
        if (dist(rng) == 0) {
            child1.push_back(parent1[i]);
            child2.push_back(parent2[i]);
        } else {
            child1.push_back(parent2[i]);
            child2.push_back(parent1[i]);
        }
    }
    
    return {child1, child2};
}

std::pair<RuleSet, RuleSet> RulesetCrossover::variableLengthCrossover(const RuleSet& parent1, const RuleSet& parent2) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double split_ratio = dist(rng);
    
    size_t split1 = static_cast<size_t>(split_ratio * parent1.size());
    size_t split2 = static_cast<size_t>(split_ratio * parent2.size());
    
    RuleSet child1, child2;
    
    // First parts
    child1.insert(child1.end(), parent1.begin(), parent1.begin() + split1);
    child2.insert(child2.end(), parent2.begin(), parent2.begin() + split2);
    
    // Second parts
    child1.insert(child1.end(), parent2.begin() + split2, parent2.end());
    child2.insert(child2.end(), parent1.begin() + split1, parent1.end());
    
    return {child1, child2};
}

// ============================================================================
// CLUSTERED CROSSOVER
// ============================================================================

void ClusteredCrossover::updateClusterInfo(const std::vector<std::pair<int, int>>& rule_pairs, 
                                         const std::vector<double>& performance_scores) {
    if (rule_pairs.size() != performance_scores.size()) {
        throw std::invalid_argument("Rule pairs and performance scores must have same size");
    }
    
    for (size_t i = 0; i < rule_pairs.size(); ++i) {
        rule_clusters[rule_pairs[i]] = performance_scores[i];
    }
}

std::pair<RuleSet, RuleSet> ClusteredCrossover::crossover(const RuleSet& parent1, const RuleSet& parent2) {
    size_t min_size = std::min(parent1.size(), parent2.size());
    RuleSet child1, child2;
    child1.reserve(min_size);
    child2.reserve(min_size);
    
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (size_t i = 0; i < min_size; ++i) {
        if (dist(rng) < 0.5) {
            child1.push_back(parent1[i]);
            child2.push_back(parent2[i]);
        } else {
            child1.push_back(parent2[i]);
            child2.push_back(parent1[i]);
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// SEGREGATION
// ============================================================================

BitString Segregation::formGamete(const MultiChromosome& parent) {
    if (parent.empty()) return {};
    
    BitString gamete;
    gamete.reserve(parent[0].size());
    
    std::uniform_int_distribution<size_t> dist(0, parent.size() - 1);
    
    for (size_t i = 0; i < parent[0].size(); ++i) {
        size_t selected_chromosome = dist(rng);
        gamete.push_back(parent[selected_chromosome][i]);
    }
    
    return gamete;
}

Segregation::MultiChromosome Segregation::crossover(const MultiChromosome& parent1, const MultiChromosome& parent2) {
    BitString gamete1 = formGamete(parent1);
    BitString gamete2 = formGamete(parent2);
    
    return {gamete1, gamete2};
}

// ============================================================================
// INTELLIGENT CROSSOVER
// ============================================================================

std::pair<BitString, BitString> IntelligentCrossover::crossover(const BitString& parent1, const BitString& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    BitString child1 = parent1;
    BitString child2 = parent2;
    
    // Try different crossover points and select best
    double best_fitness = -std::numeric_limits<double>::infinity();
    size_t best_point = 1;
    
    for (size_t point = 1; point < parent1.size(); ++point) {
        BitString temp1 = parent1;
        BitString temp2 = parent2;
        
        for (size_t i = point; i < parent1.size(); ++i) {
            temp1[i] = parent2[i];
            temp2[i] = parent1[i];
        }
        
        double fitness = fitness_function(temp1) + fitness_function(temp2);
        if (fitness > best_fitness) {
            best_fitness = fitness;
            best_point = point;
            child1 = temp1;
            child2 = temp2;
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// DISTANCE PRESERVING CROSSOVER
// ============================================================================

std::set<std::pair<int, int>> DistancePreservingCrossover::getCommonEdges(const Permutation& parent1, const Permutation& parent2) {
    std::set<std::pair<int, int>> edges1, edges2, common;
    
    // Extract edges from parent1
    for (size_t i = 0; i < parent1.size(); ++i) {
        int from = parent1[i];
        int to = parent1[(i + 1) % parent1.size()];
        edges1.insert({std::min(from, to), std::max(from, to)});
    }
    
    // Extract edges from parent2
    for (size_t i = 0; i < parent2.size(); ++i) {
        int from = parent2[i];
        int to = parent2[(i + 1) % parent2.size()];
        edges2.insert({std::min(from, to), std::max(from, to)});
    }
    
    // Find common edges
    std::set_intersection(edges1.begin(), edges1.end(),
                         edges2.begin(), edges2.end(),
                         std::inserter(common, common.begin()));
    
    return common;
}

Permutation DistancePreservingCrossover::crossover(const Permutation& parent1, const Permutation& parent2) {
    auto common_edges = getCommonEdges(parent1, parent2);
    
    Permutation child;
    std::set<int> unvisited(parent1.begin(), parent1.end());
    
    // Start from random city
    std::uniform_int_distribution<size_t> dist(0, parent1.size() - 1);
    int current = parent1[dist(rng)];
    child.push_back(current);
    unvisited.erase(current);
    
    while (!unvisited.empty()) {
        int next = findNearestUnvisited(current, unvisited);
        if (next != -1) {
            child.push_back(next);
            unvisited.erase(next);
            current = next;
        } else {
            break;
        }
    }
    
    return child;
}

// ============================================================================
// EVOLUTION STRATEGIES RECOMBINATION
// ============================================================================

RealVector DiscreteRecombination::crossover(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    RealVector child;
    child.reserve(parent1.size());
    
    std::uniform_int_distribution<int> dist(0, 1);
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        if (dist(rng) == 0) {
            child.push_back(parent1[i]);
        } else {
            child.push_back(parent2[i]);
        }
    }
    
    return child;
}

RealVector GlobalRecombination::crossover(const std::vector<RealVector>& population) {
    if (population.empty()) return {};
    
    size_t length = population[0].size();
    RealVector child;
    child.reserve(length);
    
    std::uniform_int_distribution<size_t> dist(0, population.size() - 1);
    
    for (size_t i = 0; i < length; ++i) {
        size_t selected_parent = dist(rng);
        child.push_back(population[selected_parent][i]);
    }
    
    return child;
}

// ============================================================================
// DIFFERENTIAL EVOLUTION CROSSOVER
// ============================================================================

RealVector DifferentialEvolutionCrossover::crossover(const RealVector& target, const RealVector& mutant) {
    if (target.size() != mutant.size()) {
        throw std::invalid_argument("Target and mutant must have the same length");
    }
    
    RealVector trial = target;
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::uniform_int_distribution<size_t> rand_j(0, target.size() - 1);
    
    size_t j_rand = rand_j(rng); // Ensure at least one parameter from mutant
    
    for (size_t i = 0; i < target.size(); ++i) {
        if (dist(rng) < CR || i == j_rand) {
            trial[i] = mutant[i];
        }
    }
    
    return trial;
}

// ============================================================================
// UNIFORM CROSSOVER
// ============================================================================

std::pair<BitString, BitString> UniformCrossover::crossover(const BitString& parent1, const BitString& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    BitString child1, child2;
    child1.reserve(parent1.size());
    child2.reserve(parent1.size());
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        if (dist(rng) < probability) {
            child1.push_back(parent1[i]);
            child2.push_back(parent2[i]);
        } else {
            child1.push_back(parent2[i]);
            child2.push_back(parent1[i]);
        }
    }
    
    return {child1, child2};
}

std::pair<RealVector, RealVector> UniformCrossover::crossover(const RealVector& parent1, const RealVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    RealVector child1, child2;
    child1.reserve(parent1.size());
    child2.reserve(parent1.size());
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        if (dist(rng) < probability) {
            child1.push_back(parent1[i]);
            child2.push_back(parent2[i]);
        } else {
            child1.push_back(parent2[i]);
            child2.push_back(parent1[i]);
        }
    }
    
    return {child1, child2};
}

std::pair<IntVector, IntVector> UniformCrossover::crossover(const IntVector& parent1, const IntVector& parent2) {
    if (parent1.size() != parent2.size()) {
        throw std::invalid_argument("Parents must have the same length");
    }
    
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    IntVector child1, child2;
    child1.reserve(parent1.size());
    child2.reserve(parent1.size());
    
    for (size_t i = 0; i < parent1.size(); ++i) {
        if (dist(rng) < probability) {
            child1.push_back(parent1[i]);
            child2.push_back(parent2[i]);
        } else {
            child1.push_back(parent2[i]);
            child2.push_back(parent1[i]);
        }
    }
    
    return {child1, child2};
}

// ============================================================================
// UNIFORM K-VECTOR CROSSOVER
// ============================================================================

std::vector<BitString> UniformKVectorCrossover::crossover(const std::vector<BitString>& parents) {
    if (parents.empty()) return {};
    
    size_t k = parents.size();
    size_t length = parents[0].size();
    
    // Check all parents have same length
    for (const auto& parent : parents) {
        if (parent.size() != length) {
            throw std::invalid_argument("All parents must have the same length");
        }
    }
    
    std::vector<BitString> children(k, BitString(length));
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (size_t i = 0; i < length; ++i) {
        if (dist(rng) < swap_probability) {
            // Shuffle values at position i among all parents
            std::vector<bool> values;
            for (size_t j = 0; j < k; ++j) {
                values.push_back(parents[j][i]);
            }
            std::shuffle(values.begin(), values.end(), rng);
            
            for (size_t j = 0; j < k; ++j) {
                children[j][i] = values[j];
            }
        } else {
            // Copy values directly
            for (size_t j = 0; j < k; ++j) {
                children[j][i] = parents[j][i];
            }
        }
    }
    
    return children;
}

std::vector<RealVector> UniformKVectorCrossover::crossover(const std::vector<RealVector>& parents) {
    if (parents.empty()) return {};
    
    size_t k = parents.size();
    size_t length = parents[0].size();
    
    for (const auto& parent : parents) {
        if (parent.size() != length) {
            throw std::invalid_argument("All parents must have the same length");
        }
    }
    
    std::vector<RealVector> children(k, RealVector(length));
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (size_t i = 0; i < length; ++i) {
        if (dist(rng) < swap_probability) {
            std::vector<double> values;
            for (size_t j = 0; j < k; ++j) {
                values.push_back(parents[j][i]);
            }
            std::shuffle(values.begin(), values.end(), rng);
            
            for (size_t j = 0; j < k; ++j) {
                children[j][i] = values[j];
            }
        } else {
            for (size_t j = 0; j < k; ++j) {
                children[j][i] = parents[j][i];
            }
        }
    }
    
    return children;
}

std::vector<IntVector> UniformKVectorCrossover::crossover(const std::vector<IntVector>& parents) {
    if (parents.empty()) return {};
    
    size_t k = parents.size();
    size_t length = parents[0].size();
    
    for (const auto& parent : parents) {
        if (parent.size() != length) {
            throw std::invalid_argument("All parents must have the same length");
        }
    }
    
    std::vector<IntVector> children(k, IntVector(length));
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (size_t i = 0; i < length; ++i) {
        if (dist(rng) < swap_probability) {
            std::vector<int> values;
            for (size_t j = 0; j < k; ++j) {
                values.push_back(parents[j][i]);
            }
            std::shuffle(values.begin(), values.end(), rng);
            
            for (size_t j = 0; j < k; ++j) {
                children[j][i] = values[j];
            }
        } else {
            for (size_t j = 0; j < k; ++j) {
                children[j][i] = parents[j][i];
            }
        }
    }
    
    return children;
}