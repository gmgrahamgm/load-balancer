#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

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
     * @param message The message to log
     */
    void log(const std::string& message);
    
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
