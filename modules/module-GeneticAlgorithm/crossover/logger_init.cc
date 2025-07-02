#include "crossover.h"
#include <iostream>

namespace CrossoverLogging {
    
    void initializeDefaultLogging() {
        CrossoverLogger::getInstance().initialize(
            "crossover_operations.log",  // filename
            LogLevel::INFO,              // log level
            true,                        // console output
            10 * 1024 * 1024            // 10MB max file size
        );
        
        LOG_INFO("Crossover logging system initialized with default settings");
    }
    
    void initializeDebugLogging() {
        CrossoverLogger::getInstance().initialize(
            "crossover_debug.log",
            LogLevel::DEBUG,
            true,
            50 * 1024 * 1024  // 50MB for debug logs
        );
        
        LOG_INFO("Crossover logging system initialized with debug settings");
    }
    
    void initializeProductionLogging() {
        CrossoverLogger::getInstance().initialize(
            "crossover_production.log",
            LogLevel::WARNING,  // Only warnings and errors
            false,              // No console output in production
            100 * 1024 * 1024   // 100MB for production
        );
        
        LOG_INFO("Crossover logging system initialized with production settings");
    }
    
    void logSystemStats() {
        LOG_INFO("=== Crossover System Statistics ===");
        // This could be extended to log system-wide statistics
        LOG_INFO("Logging system active and operational");
    }
    
} // namespace CrossoverLogging
