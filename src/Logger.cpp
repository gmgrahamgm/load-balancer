#include "Logger.h"
#include <sys/stat.h>
#include <sys/types.h>

Logger::Logger(const std::string& filename) : file_open(false) {
    // Create logs directory if it doesn't exist
    mkdir("logs", 0755);
    
    // Construct full path to log file in logs/ directory
    std::string filepath = "logs/" + filename;
    
    // Open log file
    file_stream.open(filepath, std::ios::out | std::ios::trunc);
    
    if (file_stream.is_open()) {
        file_open = true;
    } else {
        std::cerr << "Warning: Could not open log file: " << filepath << std::endl;
    }
}

Logger::~Logger() {
    if (file_open && file_stream.is_open()) {
        file_stream.close();
    }
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    // Write to console
    std::cout << message << std::endl;
    
    // Write to file if open
    if (file_open && file_stream.is_open()) {
        file_stream << message << std::endl;
        file_stream.flush();  // Flush for real-time viewing
    }
}

std::mutex& Logger::getMutex() {
    return log_mutex;
}
