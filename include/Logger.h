#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <mutex>
#include <string>
#include <filesystem>
#include <chrono>
#include "Common.h"

class Logger {
private:
    std::ofstream logFile_;
    mutable std::mutex mutex_;
    std::string logDir_;
    
public:
    Logger(const std::string& logDir = "~/PassFlow/Log") : logDir_(logDir) {
        // Expand ~ to home directory
        if (logDir_[0] == '~') {
            const char* home = getenv("HOME");
            if (home) {
                logDir_ = std::string(home) + logDir_.substr(1);
            }
        }
        
        // Create log directory if it doesn't exist
        std::filesystem::create_directories(logDir_);
        
        // Open log file
        std::string filename = logDir_ + "/passflow_" + getCurrentDateString() + ".log";
        logFile_.open(filename, std::ios::app);
        
        if (!logFile_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + filename);
        }
    }
    
    ~Logger() {
        if (logFile_.is_open()) {
            logFile_.close();
        }
    }
    
    // Log a command with timestamp
    void logCommand(const std::string& command) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        logFile_ << formatTimestamp(now) << " - " << command << std::endl;
        logFile_.flush();
    }
    
    // Log general message
    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        logFile_ << formatTimestamp(now) << " - " << message << std::endl;
        logFile_.flush();
    }
    
    // Log error message
    void logError(const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        logFile_ << formatTimestamp(now) << " - ERROR: " << error << std::endl;
        logFile_.flush();
    }
};

#endif // LOGGER_H
