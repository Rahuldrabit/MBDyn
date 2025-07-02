#include "mutation.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <numeric>
#include <iomanip>
#include <chrono>
#include <ctime>

// ======================== LOGGER IMPLEMENTATION ========================

void ConsoleLogger::log(Level level, const std::string& message) {
    if (level < minLevel) return;
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::string levelStr;
    switch (level) {
        case DEBUG: levelStr = "DEBUG"; break;
        case INFO: levelStr = "INFO"; break;
        case WARNING: levelStr = "WARN"; break;
        case ERROR: levelStr = "ERROR"; break;
    }
    
    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
              << "] [" << levelStr << "] " << message << std::endl;
}

// ======================== MUTATION OPERATORS IMPLEMENTATION ========================

MutationOperators::MutationOperators(std::shared_ptr<Logger> logger) 
    : rng(std::random_device{}()), 
      uniform_dist(0.0, 1.0), 
      normal_dist(0.0, 1.0),
      logger(logger) {
    
    if (!this->logger) {
        this->logger = std::make_shared<ConsoleLogger>(Logger::INFO);
    }
    
    this->logger->info("MutationOperators initialized with seed: " + 
                      std::to_string(std::random_device{}()));
}

void MutationOperators::validateProbability(double pm, const std::string& methodName) const {
    if (pm < 0.0 || pm > 1.0) {
        std::string msg = methodName + " - probability must be in [0,1], got: " + std::to_string(pm);
        logger->error(msg);
        throw InvalidParameterException(msg);
    }
}

void MutationOperators::validateBounds(const std::vector<double>& lower, 
                                      const std::vector<double>& upper) const {
    if (lower.size() != upper.size()) {
        std::string msg = "Lower and upper bounds size mismatch: " + 
                         std::to_string(lower.size()) + " vs " + std::to_string(upper.size());
        logger->error(msg);
        throw InvalidParameterException(msg);
    }
    
    for (size_t i = 0; i < lower.size(); ++i) {
        if (lower[i] > upper[i]) {
            std::string msg = "Invalid bounds at index " + std::to_string(i) + 
                             ": lower=" + std::to_string(lower[i]) + 
                             " > upper=" + std::to_string(upper[i]);
            logger->error(msg);
            throw InvalidParameterException(msg);
        }
    }
}

void MutationOperators::logMutationAttempt(const std::string& methodName, double pm, 
                                          size_t chromosomeSize) const {
    std::ostringstream oss;
    oss << methodName << " - Attempting mutation with pm=" << pm 
        << ", chromosome_size=" << chromosomeSize;
    logger->debug(oss.str());
}

void MutationOperators::logMutationResult(const std::string& methodName, bool success, 
                                         const std::string& details) const {
    std::ostringstream oss;
    oss << methodName << " - " << (success ? "Success" : "Failed");
    if (!details.empty()) {
        oss << " (" << details << ")";
    }
    logger->debug(oss.str());
}

// ======================== BINARY REPRESENTATION ========================

void MutationOperators::bitFlipMutation(std::vector<bool>& chromosome, double pm) const {
    validateProbability(pm, "bitFlipMutation");
    logMutationAttempt("bitFlipMutation", pm, chromosome.size());
    
    ++stats.totalMutations;
    size_t mutations = 0;
    
    try {
        for (size_t i = 0; i < chromosome.size(); ++i) {
            if (uniform_dist(rng) < pm) {
                chromosome[i] = !chromosome[i];
                ++mutations;
            }
        }
        
        ++stats.successfulMutations;
        logMutationResult("bitFlipMutation", true, std::to_string(mutations) + " bits flipped");
        
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("bitFlipMutation failed: " + std::string(e.what()));
        throw;
    }
}

void MutationOperators::bitFlipMutation(std::string& binaryString, double pm) const {
    validateProbability(pm, "bitFlipMutation(string)");
    logMutationAttempt("bitFlipMutation(string)", pm, binaryString.size());
    
    ++stats.totalMutations;
    size_t mutations = 0;
    
    try {
        for (size_t i = 0; i < binaryString.size(); ++i) {
            if (uniform_dist(rng) < pm) {
                binaryString[i] = (binaryString[i] == '0') ? '1' : '0';
                ++mutations;
            }
        }
        
        ++stats.successfulMutations;
        logMutationResult("bitFlipMutation(string)", true, std::to_string(mutations) + " bits flipped");
        
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("bitFlipMutation(string) failed: " + std::string(e.what()));
        throw;
    }
}

// ======================== INTEGER REPRESENTATION ========================

void MutationOperators::randomResettingMutation(std::vector<int>& chromosome, double pm, 
                                               int minVal, int maxVal) const {
    validateProbability(pm, "randomResettingMutation");
    
    if (minVal > maxVal) {
        std::string msg = "Invalid range: minVal=" + std::to_string(minVal) + 
                         " > maxVal=" + std::to_string(maxVal);
        logger->error(msg);
        throw InvalidParameterException(msg);
    }
    
    logMutationAttempt("randomResettingMutation", pm, chromosome.size());
    
    ++stats.totalMutations;
    size_t mutations = 0;
    
    try {
        std::uniform_int_distribution<int> int_dist(minVal, maxVal);
        
        for (size_t i = 0; i < chromosome.size(); ++i) {
            if (uniform_dist(rng) < pm) {
                int oldVal = chromosome[i];
                chromosome[i] = int_dist(rng);
                ++mutations;
                
                logger->debug("Gene " + std::to_string(i) + " changed from " + 
                             std::to_string(oldVal) + " to " + std::to_string(chromosome[i]));
            }
        }
        
        ++stats.successfulMutations;
        logMutationResult("randomResettingMutation", true, std::to_string(mutations) + " genes reset");
        
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("randomResettingMutation failed: " + std::string(e.what()));
        throw;
    }
}

void MutationOperators::creepMutation(std::vector<int>& chromosome, double pm, 
                                     int stepSize, int minVal, int maxVal) const {
    validateProbability(pm, "creepMutation");
    
    if (minVal > maxVal || stepSize < 0) {
        std::string msg = "Invalid parameters: minVal=" + std::to_string(minVal) + 
                         ", maxVal=" + std::to_string(maxVal) + 
                         ", stepSize=" + std::to_string(stepSize);
        logger->error(msg);
        throw InvalidParameterException(msg);
    }
    
    logMutationAttempt("creepMutation", pm, chromosome.size());
    
    ++stats.totalMutations;
    size_t mutations = 0;
    double totalPerturbation = 0.0;
    
    try {
        std::uniform_int_distribution<int> step_dist(-stepSize, stepSize);
        
        for (size_t i = 0; i < chromosome.size(); ++i) {
            if (uniform_dist(rng) < pm) {
                int oldVal = chromosome[i];
                int newVal = chromosome[i] + step_dist(rng);
                chromosome[i] = std::max(minVal, std::min(maxVal, newVal));
                ++mutations;
                
                totalPerturbation += std::abs(chromosome[i] - oldVal);
                
                logger->debug("Gene " + std::to_string(i) + " crept from " + 
                             std::to_string(oldVal) + " to " + std::to_string(chromosome[i]));
            }
        }
        
        if (mutations > 0) {
            stats.averagePerturbation = (stats.averagePerturbation * stats.successfulMutations + 
                                       totalPerturbation / mutations) / (stats.successfulMutations + 1);
        }
        
        ++stats.successfulMutations;
        logMutationResult("creepMutation", true, std::to_string(mutations) + " genes mutated");
        
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("creepMutation failed: " + std::string(e.what()));
        throw;
    }
}

// ======================== REAL-VALUED REPRESENTATION ========================

void MutationOperators::uniformMutation(std::vector<double>& chromosome, double pm,
                                       const std::vector<double>& lowerBounds,
                                       const std::vector<double>& upperBounds) const {
    validateProbability(pm, "uniformMutation");
    validateBounds(lowerBounds, upperBounds);
    
    if (chromosome.size() != lowerBounds.size()) {
        std::string msg = "Chromosome size mismatch with bounds: " + 
                         std::to_string(chromosome.size()) + " vs " + std::to_string(lowerBounds.size());
        logger->error(msg);
        throw InvalidParameterException(msg);
    }
    
    logMutationAttempt("uniformMutation", pm, chromosome.size());
    
    ++stats.totalMutations;
    size_t mutations = 0;
    
    try {
        for (size_t i = 0; i < chromosome.size(); ++i) {
            if (uniform_dist(rng) < pm) {
                double oldVal = chromosome[i];
                std::uniform_real_distribution<double> range_dist(lowerBounds[i], upperBounds[i]);
                chromosome[i] = range_dist(rng);
                ++mutations;
                
                logger->debug("Gene " + std::to_string(i) + " uniformly mutated from " + 
                             std::to_string(oldVal) + " to " + std::to_string(chromosome[i]));
            }
        }
        
        ++stats.successfulMutations;
        logMutationResult("uniformMutation", true, std::to_string(mutations) + " genes mutated");
        
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("uniformMutation failed: " + std::string(e.what()));
        throw;
    }
}

void MutationOperators::gaussianMutation(std::vector<double>& chromosome, double pm, 
                                        double sigma, const std::vector<double>& lowerBounds,
                                        const std::vector<double>& upperBounds) const {
    validateProbability(pm, "gaussianMutation");
    validateBounds(lowerBounds, upperBounds);
    
    if (sigma <= 0.0) {
        std::string msg = "Sigma must be positive, got: " + std::to_string(sigma);
        logger->error(msg);
        throw InvalidParameterException(msg);
    }
    
    if (chromosome.size() != lowerBounds.size()) {
        std::string msg = "Chromosome size mismatch with bounds";
        logger->error(msg);
        throw InvalidParameterException(msg);
    }
    
    logMutationAttempt("gaussianMutation", pm, chromosome.size());
    
    ++stats.totalMutations;
    size_t mutations = 0;
    double totalPerturbation = 0.0;
    
    try {
        std::normal_distribution<double> gauss_dist(0.0, sigma);
        
        for (size_t i = 0; i < chromosome.size(); ++i) {
            if (uniform_dist(rng) < pm) {
                double oldVal = chromosome[i];
                double perturbation = gauss_dist(rng);
                chromosome[i] += perturbation;
                
                // Clamp to valid range
                chromosome[i] = std::max(lowerBounds[i], 
                                       std::min(upperBounds[i], chromosome[i]));
                ++mutations;
                
                totalPerturbation += std::abs(perturbation);
                
                logger->debug("Gene " + std::to_string(i) + " Gaussian mutated from " + 
                             std::to_string(oldVal) + " to " + std::to_string(chromosome[i]) +
                             " (perturbation: " + std::to_string(perturbation) + ")");
            }
        }
        
        if (mutations > 0) {
            stats.averagePerturbation = (stats.averagePerturbation * stats.successfulMutations + 
                                       totalPerturbation / mutations) / (stats.successfulMutations + 1);
        }
        
        ++stats.successfulMutations;
        logMutationResult("gaussianMutation", true, 
                         std::to_string(mutations) + " genes mutated, avg_perturbation=" + 
                         std::to_string(totalPerturbation / std::max(1.0, static_cast<double>(mutations))));
        
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("gaussianMutation failed: " + std::string(e.what()));
        throw;
    }
}

MutationOperators::SelfAdaptiveIndividual::SelfAdaptiveIndividual(size_t size, double initial_sigma) 
    : genes(size), sigma(initial_sigma) {
    if (initial_sigma <= 0.0) {
        throw InvalidParameterException("Initial sigma must be positive");
    }
}

void MutationOperators::selfAdaptiveMutation(SelfAdaptiveIndividual& individual,
                                            const std::vector<double>& lowerBounds,
                                            const std::vector<double>& upperBounds,
                                            double tau) const {
    validateBounds(lowerBounds, upperBounds);
    
    if (tau <= 0.0) {
        std::string msg = "Tau must be positive, got: " + std::to_string(tau);
        logger->error(msg);
        throw InvalidParameterException(msg);
    }
    
    logger->debug("selfAdaptiveMutation - current sigma: " + std::to_string(individual.sigma));
    
    ++stats.totalMutations;
    
    try {
        // Mutate step size first
        double gamma = normal_dist(rng);
        double oldSigma = individual.sigma;
        individual.sigma *= std::exp(tau * gamma);
        
        // Ensure sigma doesn't become too small or too large
        individual.sigma = std::max(individual.sigma, 1e-6);
        individual.sigma = std::min(individual.sigma, 100.0);
        
        logger->debug("Sigma updated from " + std::to_string(oldSigma) + 
                     " to " + std::to_string(individual.sigma));
        
        // Mutate genes using the updated sigma
        std::normal_distribution<double> gene_dist(0.0, individual.sigma);
        size_t mutations = 0;
        
        for (size_t i = 0; i < individual.genes.size(); ++i) {
            double oldVal = individual.genes[i];
            individual.genes[i] += gene_dist(rng);
            
            // Clamp to valid range
            individual.genes[i] = std::max(lowerBounds[i], 
                                         std::min(upperBounds[i], individual.genes[i]));
            
            if (std::abs(individual.genes[i] - oldVal) > 1e-10) {
                ++mutations;
            }
        }
        
        ++stats.successfulMutations;
        logMutationResult("selfAdaptiveMutation", true, 
                         std::to_string(mutations) + " genes affected, new_sigma=" + 
                         std::to_string(individual.sigma));
        
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("selfAdaptiveMutation failed: " + std::string(e.what()));
        throw;
    }
}

// ======================== PERMUTATION REPRESENTATION ========================

void MutationOperators::swapMutation(std::vector<int>& permutation, double pm) const {
    validateProbability(pm, "swapMutation");
    
    if (permutation.empty()) {
        logger->warning("swapMutation - empty permutation");
        return;
    }
    
    logMutationAttempt("swapMutation", pm, permutation.size());
    
    ++stats.totalMutations;
    
    try {
        if (uniform_dist(rng) < pm && permutation.size() > 1) {
            std::uniform_int_distribution<size_t> pos_dist(0, permutation.size() - 1);
            size_t pos1 = pos_dist(rng);
            size_t pos2 = pos_dist(rng);
            
            // Ensure different positions
            size_t attempts = 0;
            while (pos1 == pos2 && attempts < 100) {
                pos2 = pos_dist(rng);
                ++attempts;
            }
            
            if (pos1 != pos2) {
                std::swap(permutation[pos1], permutation[pos2]);
                logger->debug("Swapped positions " + std::to_string(pos1) + 
                             " and " + std::to_string(pos2));
                ++stats.successfulMutations;
                logMutationResult("swapMutation", true, "positions " + std::to_string(pos1) + 
                                 " and " + std::to_string(pos2) + " swapped");
            } else {
                logger->warning("swapMutation - could not find different positions");
                ++stats.failedMutations;
            }
        } else {
            logMutationResult("swapMutation", true, "no mutation applied");
            ++stats.successfulMutations;
        }
        
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("swapMutation failed: " + std::string(e.what()));
        throw;
    }
}

void MutationOperators::insertMutation(std::vector<int>& permutation, double pm) const {
    validateProbability(pm, "insertMutation");
    logMutationAttempt("insertMutation", pm, permutation.size());
    
    ++stats.totalMutations;
    
    try {
        if (uniform_dist(rng) < pm && permutation.size() > 1) {
            std::uniform_int_distribution<size_t> pos_dist(0, permutation.size() - 1);
            size_t pos1 = pos_dist(rng);
            size_t pos2 = pos_dist(rng);
            
            if (pos1 != pos2) {
                int value = permutation[pos1];
                permutation.erase(permutation.begin() + pos1);
                
                // Adjust pos2 if necessary
                if (pos2 > pos1) pos2--;
                
                permutation.insert(permutation.begin() + pos2, value);
                ++stats.successfulMutations;
                logMutationResult("insertMutation", true, "element moved from " + 
                                 std::to_string(pos1) + " to " + std::to_string(pos2));
            } else {
                ++stats.successfulMutations;
                logMutationResult("insertMutation", true, "no change - same positions");
            }
        } else {
            ++stats.successfulMutations;
            logMutationResult("insertMutation", true, "no mutation applied");
        }
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("insertMutation failed: " + std::string(e.what()));
        throw;
    }
}

void MutationOperators::scrambleMutation(std::vector<int>& permutation, double pm) const {
    validateProbability(pm, "scrambleMutation");
    logMutationAttempt("scrambleMutation", pm, permutation.size());
    
    ++stats.totalMutations;
    
    try {
        if (uniform_dist(rng) < pm && permutation.size() > 1) {
            std::uniform_int_distribution<size_t> pos_dist(0, permutation.size() - 1);
            size_t start = pos_dist(rng);
            size_t end = pos_dist(rng);
            
            if (start > end) std::swap(start, end);
            
            // Scramble the subsequence
            std::shuffle(permutation.begin() + start, 
                        permutation.begin() + end + 1, rng);
            
            ++stats.successfulMutations;
            logMutationResult("scrambleMutation", true, "scrambled region [" + 
                             std::to_string(start) + ", " + std::to_string(end) + "]");
        } else {
            ++stats.successfulMutations;
            logMutationResult("scrambleMutation", true, "no mutation applied");
        }
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("scrambleMutation failed: " + std::string(e.what()));
        throw;
    }
}

void MutationOperators::inversionMutation(std::vector<int>& permutation, double pm) const {
    validateProbability(pm, "inversionMutation");
    logMutationAttempt("inversionMutation", pm, permutation.size());
    
    ++stats.totalMutations;
    
    try {
        if (uniform_dist(rng) < pm && permutation.size() > 1) {
            std::uniform_int_distribution<size_t> pos_dist(0, permutation.size() - 1);
            size_t start = pos_dist(rng);
            size_t end = pos_dist(rng);
            
            if (start > end) std::swap(start, end);
            
            // Reverse the subsequence
            std::reverse(permutation.begin() + start, 
                        permutation.begin() + end + 1);
            
            ++stats.successfulMutations;
            logMutationResult("inversionMutation", true, "inverted region [" + 
                             std::to_string(start) + ", " + std::to_string(end) + "]");
        } else {
            ++stats.successfulMutations;
            logMutationResult("inversionMutation", true, "no mutation applied");
        }
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("inversionMutation failed: " + std::string(e.what()));
        throw;
    }
}

// ======================== LIST REPRESENTATION ========================

void MutationOperators::listMutation(std::vector<int>& list, double pm_content, double pm_size,
                                     int minVal, int maxVal, size_t minSize, size_t maxSize) const {
    validateProbability(pm_content, "listMutation(content)");
    validateProbability(pm_size, "listMutation(size)");
    
    if (minVal > maxVal || minSize > maxSize) {
        std::string msg = "Invalid parameters for listMutation";
        logger->error(msg);
        throw InvalidParameterException(msg);
    }
    
    logMutationAttempt("listMutation", pm_content, list.size());
    
    ++stats.totalMutations;
    size_t contentMutations = 0;
    bool sizeMutated = false;
    
    try {
        // Mutate content first
        std::uniform_int_distribution<int> val_dist(minVal, maxVal);
        for (size_t i = 0; i < list.size(); ++i) {
            if (uniform_dist(rng) < pm_content) {
                int oldVal = list[i];
                list[i] = val_dist(rng);
                ++contentMutations;
                logger->debug("List element " + std::to_string(i) + " changed from " + 
                             std::to_string(oldVal) + " to " + std::to_string(list[i]));
            }
        }
        
        // Mutate size
        size_t oldSize = list.size();
        if (uniform_dist(rng) < pm_size) {
            std::uniform_int_distribution<int> size_change(-1, 1);
            int change = size_change(rng);
            
            size_t newSize = list.size() + change;
            newSize = std::max(minSize, std::min(maxSize, newSize));
            
            if (newSize > list.size()) {
                // Add random elements
                for (size_t i = list.size(); i < newSize; ++i) {
                    list.push_back(val_dist(rng));
                }
                sizeMutated = true;
            } else if (newSize < list.size()) {
                // Remove elements
                list.resize(newSize);
                sizeMutated = true;
            }
        }
        
        ++stats.successfulMutations;
        std::string details = std::to_string(contentMutations) + " content mutations";
        if (sizeMutated) {
            details += ", size changed from " + std::to_string(oldSize) + " to " + std::to_string(list.size());
        }
        logMutationResult("listMutation", true, details);
        
    } catch (const std::exception& e) {
        ++stats.failedMutations;
        logger->error("listMutation failed: " + std::string(e.what()));
        throw;
    }
}

// ======================== UTILITY FUNCTIONS ========================

void MutationOperators::setSeed(unsigned int seed) {
    rng.seed(seed);
    logger->info("Random seed set to: " + std::to_string(seed));
}

void MutationOperators::setLogger(std::shared_ptr<Logger> newLogger) {
    if (newLogger) {
        logger = newLogger;
        logger->info("Logger updated for MutationOperators");
    }
}

std::vector<bool> MutationOperators::generateBinaryChromosome(size_t length) const {
    logger->debug("Generating binary chromosome of length " + std::to_string(length));
    std::vector<bool> chromosome(length);
    for (size_t i = 0; i < length; ++i) {
        chromosome[i] = uniform_dist(rng) < 0.5;
    }
    return chromosome;
}

std::vector<int> MutationOperators::generateIntegerChromosome(size_t length, int minVal, int maxVal) const {
    logger->debug("Generating integer chromosome of length " + std::to_string(length));
    std::vector<int> chromosome(length);
    std::uniform_int_distribution<int> int_dist(minVal, maxVal);
    for (size_t i = 0; i < length; ++i) {
        chromosome[i] = int_dist(rng);
    }
    return chromosome;
}

std::vector<double> MutationOperators::generateRealChromosome(size_t length, 
                                                             const std::vector<double>& lowerBounds,
                                                             const std::vector<double>& upperBounds) const {
    logger->debug("Generating real chromosome of length " + std::to_string(length));
    validateBounds(lowerBounds, upperBounds);
    
    if (length != lowerBounds.size()) {
        throw InvalidParameterException("Length mismatch with bounds");
    }
    
    std::vector<double> chromosome(length);
    for (size_t i = 0; i < length; ++i) {
        std::uniform_real_distribution<double> range_dist(lowerBounds[i], upperBounds[i]);
        chromosome[i] = range_dist(rng);
    }
    return chromosome;
}

std::vector<int> MutationOperators::generatePermutation(int n) const {
    logger->debug("Generating permutation of size " + std::to_string(n));
    if (n < 0) {
        throw InvalidParameterException("Permutation size must be non-negative");
    }
    
    std::vector<int> permutation(n);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::shuffle(permutation.begin(), permutation.end(), rng);
    return permutation;
}