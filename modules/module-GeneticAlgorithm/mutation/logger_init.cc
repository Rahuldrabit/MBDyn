#include "mutation.h"
#include <iostream>
#include <memory>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <cstdio>

namespace MutationLogging {

    /**
     * @brief File logger implementation for mutation operations
     */
    class FileLogger : public Logger {
    private:
        std::ofstream logFile;
        Logger::Level minLevel;
        bool consoleOutput;
        size_t maxFileSize;
        size_t currentFileSize;
        std::string filename;
        
        void rotateLogFile() {
            if (logFile.is_open()) {
                logFile.close();
            }
            
            // Rename current log file
            std::string backupName = filename + ".bak";
            std::rename(filename.c_str(), backupName.c_str());
            
            // Open new log file
            logFile.open(filename, std::ios::app);
            currentFileSize = 0;
            
            if (logFile.is_open()) {
                log(Logger::INFO, "Log file rotated");
            }
        }
        
        std::string getCurrentTimestamp() const {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            std::ostringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
            return ss.str();
        }
        
        std::string levelToString(Logger::Level level) const {
            switch (level) {
                case Logger::DEBUG: return "DEBUG";
                case Logger::INFO: return "INFO";
                case Logger::WARNING: return "WARNING";
                case Logger::ERROR: return "ERROR";
                default: return "UNKNOWN";
            }
        }
        
    public:
        FileLogger(const std::string& filename, Logger::Level minLevel = Logger::INFO, 
                  bool console = true, size_t maxSize = 10 * 1024 * 1024)
            : minLevel(minLevel), consoleOutput(console), maxFileSize(maxSize), 
              currentFileSize(0), filename(filename) {
            
            logFile.open(filename, std::ios::app);
            if (logFile.is_open()) {
                // Get current file size
                logFile.seekp(0, std::ios::end);
                currentFileSize = logFile.tellp();
                log(Logger::INFO, "FileLogger initialized for mutation operations");
            } else {
                std::cerr << "Failed to open log file: " << filename << std::endl;
            }
        }
        
        ~FileLogger() {
            if (logFile.is_open()) {
                log(Logger::INFO, "FileLogger shutting down");
                logFile.close();
            }
        }
        
        void log(Logger::Level level, const std::string& message) override {
            if (level < minLevel) return;
            
            std::ostringstream logEntry;
            logEntry << "[" << getCurrentTimestamp() << "] "
                    << "[" << levelToString(level) << "] "
                    << "[MUTATION] " << message;
            
            std::string fullMessage = logEntry.str();
            
            // Console output
            if (consoleOutput) {
                if (level >= Logger::ERROR) {
                    std::cerr << fullMessage << std::endl;
                } else {
                    std::cout << fullMessage << std::endl;
                }
            }
            
            // File output
            if (logFile.is_open()) {
                // Check if rotation is needed
                if (currentFileSize + fullMessage.length() > maxFileSize) {
                    rotateLogFile();
                }
                
                logFile << fullMessage << std::endl;
                logFile.flush();
                currentFileSize += fullMessage.length() + 1; // +1 for newline
            }
        }
        
        void setConsoleOutput(bool enable) { consoleOutput = enable; }
        void setMinLevel(Logger::Level level) { minLevel = level; }
    };

    /**
     * @brief Initialize default logging configuration for mutation operations
     */
    std::shared_ptr<Logger> initializeDefaultLogging() {
        auto logger = std::make_shared<FileLogger>(
            "mutation_operations.log",     // filename
            Logger::INFO,                  // log level
            true,                         // console output
            10 * 1024 * 1024             // 10MB max file size
        );
        
        logger->info("Mutation logging system initialized with default settings");
        logger->info("Log file: mutation_operations.log");
        logger->info("Log level: INFO");
        logger->info("Console output: enabled");
        logger->info("Max file size: 10MB");
        
        return logger;
    }
    
    /**
     * @brief Initialize debug logging configuration for development
     */
    std::shared_ptr<Logger> initializeDebugLogging() {
        auto logger = std::make_shared<FileLogger>(
            "mutation_debug.log",
            Logger::DEBUG,
            true,
            50 * 1024 * 1024  // 50MB for debug logs
        );
        
        logger->info("Mutation logging system initialized with debug settings");
        logger->info("All mutation operations will be logged in detail");
        logger->debug("Debug logging enabled - verbose output active");
        
        return logger;
    }
    
    /**
     * @brief Initialize production logging configuration
     */
    std::shared_ptr<Logger> initializeProductionLogging() {
        auto logger = std::make_shared<FileLogger>(
            "mutation_production.log",
            Logger::WARNING,  // Only warnings and errors
            false,           // No console output in production
            100 * 1024 * 1024 // 100MB for production
        );
        
        logger->info("Mutation logging system initialized with production settings");
        logger->info("Only warnings and errors will be logged");
        
        return logger;
    }
    
    /**
     * @brief Create a custom logger with specified parameters
     */
    std::shared_ptr<Logger> createCustomLogger(const std::string& filename,
                                              Logger::Level level,
                                              bool consoleOutput,
                                              size_t maxFileSize) {
        auto logger = std::make_shared<FileLogger>(filename, level, consoleOutput, maxFileSize);
        
        std::ostringstream msg;
        msg << "Custom mutation logger created - file: " << filename 
            << ", level: " << static_cast<int>(level)
            << ", console: " << (consoleOutput ? "enabled" : "disabled")
            << ", max_size: " << maxFileSize << " bytes";
        logger->info(msg.str());
        
        return logger;
    }
    
    /**
     * @brief Performance-oriented logger for high-frequency operations
     */
    class PerformanceLogger : public Logger {
    private:
        std::ofstream logFile;
        Logger::Level minLevel;
        size_t messageCount;
        std::chrono::high_resolution_clock::time_point startTime;
        
    public:
        PerformanceLogger(const std::string& filename, Logger::Level minLevel = Logger::WARNING)
            : minLevel(minLevel), messageCount(0) {
            logFile.open(filename, std::ios::app);
            startTime = std::chrono::high_resolution_clock::now();
            if (logFile.is_open()) {
                log(Logger::INFO, "PerformanceLogger started for mutation operations");
            }
        }
        
        ~PerformanceLogger() {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - startTime).count();
            
            if (logFile.is_open()) {
                logFile << "Session summary: " << messageCount << " messages in " 
                       << duration << "ms" << std::endl;
                logFile.close();
            }
        }
        
        void log(Logger::Level level, const std::string& message) override {
            if (level < minLevel) return;
            
            ++messageCount;
            
            if (logFile.is_open()) {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - startTime).count();
                
                logFile << elapsed << "us [" << static_cast<int>(level) << "] " 
                       << message << std::endl;
                
                // Flush only for errors to maintain performance
                if (level >= Logger::ERROR) {
                    logFile.flush();
                }
            }
        }
    };
    
    /**
     * @brief Create a performance-oriented logger for high-frequency operations
     */
    std::shared_ptr<Logger> createPerformanceLogger(const std::string& filename,
                                                   Logger::Level minLevel = Logger::WARNING) {
        return std::make_shared<PerformanceLogger>(filename, minLevel);
    }
    
} // namespace MutationLogging
    class PerformanceLogger : public Logger {
    private:
        std::ofstream logFile;
        Level minLevel;
        size_t messageCount;
        std::chrono::high_resolution_clock::time_point startTime;
        
    public:
        PerformanceLogger(const std::string& filename, Level minLevel = WARNING)
            : minLevel(minLevel), messageCount(0) {
            logFile.open(filename, std::ios::app);
            startTime = std::chrono::high_resolution_clock::now();
            if (logFile.is_open()) {
                log(INFO, "PerformanceLogger started for mutation operations");
            }
        }
        
        ~PerformanceLogger() {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - startTime).count();
            
            if (logFile.is_open()) {
                logFile << "Session summary: " << messageCount << " messages in " 
                       << duration << "ms" << std::endl;
                logFile.close();
            }
        }
        
        void log(Level level, const std::string& message) override {
            if (level < minLevel) return;
            
            ++messageCount;
            
            if (logFile.is_open()) {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - startTime).count();
                
                logFile << elapsed << "us [" << static_cast<int>(level) << "] " 
                       << message << std::endl;
                
                // Flush only for errors to maintain performance
                if (level >= ERROR) {
                    logFile.flush();
                }
            }
        }
    };
    
    /**
     * @brief Create a performance-oriented logger for high-frequency operations
     */
    std::shared_ptr<Logger> createPerformanceLogger(const std::string& filename,
                                                   Logger::Level minLevel = Logger::WARNING) {
        return std::make_shared<PerformanceLogger>(filename, minLevel);
    }
    

