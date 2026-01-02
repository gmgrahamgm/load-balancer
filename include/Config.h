#ifndef CONFIG_H
#define CONFIG_H

#include <string>

/**
 * Configuration parameters for the load balancer simulation.
 * Loads settings from a key=value format file.
 */
class Config {
public:
    // Load balancer settings
    int num_loadbalancers;
    int initial_servers;
    int runtime_cycles;
    
    // Scaling thresholds
    int scaling_threshold_high;  // Add server when queue > threshold_high * num_servers
    int scaling_threshold_low;   // Remove server when queue < threshold_low * num_servers
    int scaling_cooldown;        // Wait N cycles between scaling operations
    
    // Server limits
    int min_servers;
    int max_servers;
    
    // Request generation parameters
    int request_gen_interval_min;  // Min cycles between requests
    int request_gen_interval_max;  // Max cycles between requests
    
    // Request processing time ranges
    int min_processing_time;
    int max_processing_time;
    
    // Constructor with default values
    Config();
    
    /**
     * Load configuration from file.
     * Returns true on success, false on error.
     */
    bool loadFromFile(const std::string& filename);
    
    /**
     * Validate configuration parameters.
     * Returns true if all values are valid, false otherwise.
     */
    bool validate() const;
    
private:
    // Parse a single line from config file
    bool parseLine(const std::string& line);
};

#endif // CONFIG_H
