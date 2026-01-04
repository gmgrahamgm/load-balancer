#include "Logger.h"
#include <sys/stat.h>
#include <sys/types.h>

Logger::Logger(const std::string& filename) : file_open(false), current_log_level(0) {
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

void Logger::log(const std::string& message, LogColor color) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    // Write to console with optional color
    // Using softer ANSI colors for comfortable viewing
    const char* color_code = "";
    const char* reset_code = "\033[0m";
    
    switch (color) {
        case LogColor::WHITE:
            color_code = "\033[37m";  // Normal white
            break;
        case LogColor::BLUE:
            color_code = "\033[94m";  // Bright blue
            break;
        case LogColor::ORANGE:
            color_code = "\033[93m";  // Bright yellow/orange
            break;
        case LogColor::RED:
            color_code = "\033[91m";  // Bright red
            break;
        case LogColor::NONE:
        default:
            color_code = "";
            reset_code = "";
            break;
    }
    
    std::cout << color_code << message << reset_code << std::endl;
    
    // Write to file as plain text (no color codes)
    if (file_open && file_stream.is_open()) {
        file_stream << message << std::endl;
        file_stream.flush();  // Flush for real-time viewing
    }
}

std::mutex& Logger::getMutex() {
    return log_mutex;
}

void Logger::setLogLevel(int level) {
    current_log_level = level;
}

int Logger::getLogLevel() const {
    return current_log_level;
}
