#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

/**
 * Color options for console output.
 * Log files always receive plain text regardless of color choice.
 */
enum class LogColor {
    NONE,    // Default terminal color
    WHITE,   // Bright white
    BLUE,    // Bright blue (scale up events)
    ORANGE,  // Bright yellow/orange (scale down events)
    RED      // Bright red (blocked IP events)
};

/**
 * Thread-safe Logger that writes to both console and file.
 * Handles all logging output for the load balancer simulation.
 */
class Logger {
public:
    /**
     * Constructor: Opens log file for writing
     * @param filename Path to log file (creates directories if needed)
     */
    explicit Logger(const std::string& filename);
    
    /**
     * Destructor: Closes log file
     */
    ~Logger();
    
    /**
     * Write a message to both console and log file (thread-safe)
     * Console output can be colored; file output is always plain text
     * @param message The message to log
     * @param color Optional color for console output (default: NONE)
     */
    void log(const std::string& message, LogColor color = LogColor::NONE);
    
    /**
     * Set the log level
     * @param level Log level (0=VERBOSE, 1=PERIODIC, 2=QUIET)
     */
    void setLogLevel(int level);
    
    /**
     * Get the current log level
     * @return Current log level
     */
    int getLogLevel() const;
    
    /**
     * Get the internal mutex for external synchronization if needed
     * (For backward compatibility with code that needs direct mutex access)
     */
    std::mutex& getMutex();
    
private:
    std::ofstream file_stream;
    std::mutex log_mutex;
    bool file_open;
    int current_log_level;
};

#endif // LOGGER_H
