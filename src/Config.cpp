#include "Config.h"
#include <fstream>
#include <sstream>
#include <iostream>

Config::Config() {
    // Set default values
    num_loadbalancers = 1;
    initial_servers = 10;
    runtime_cycles = 10000;
    scaling_threshold_high = 25;
    scaling_threshold_low = 15;
    scaling_cooldown = 100;
    min_servers = 5;
    max_servers = 50;
    request_gen_interval_min = 5;
    request_gen_interval_max = 20;
    min_processing_time = 10;
    max_processing_time = 100;
}

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open config file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    int line_number = 0;
    
    while (std::getline(file, line)) {
        line_number++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        if (!parseLine(line)) {
            std::cerr << "Error: Invalid config line " << line_number << ": " << line << std::endl;
            return false;
        }
    }
    
    file.close();
    
    return true;
}

bool Config::parseLine(const std::string& line) {
    // "=" delimiter
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return false;
    }
    
    // Extract key and value
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    
    // Trim whitespace from key
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    
    // Trim whitespace from value
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    // Convert value to integer
    int int_value;
    try {
        int_value = std::stoi(value);
    } catch (...) {
        return false;
    }
    
    // Assign to appropriate field
    if (key == "num_loadbalancers") {
        num_loadbalancers = int_value;
    } else if (key == "initial_servers") {
        initial_servers = int_value;
    } else if (key == "runtime_cycles") {
        runtime_cycles = int_value;
    } else if (key == "scaling_threshold_high") {
        scaling_threshold_high = int_value;
    } else if (key == "scaling_threshold_low") {
        scaling_threshold_low = int_value;
    } else if (key == "scaling_cooldown") {
        scaling_cooldown = int_value;
    } else if (key == "min_servers") {
        min_servers = int_value;
    } else if (key == "max_servers") {
        max_servers = int_value;
    } else if (key == "request_gen_interval_min") {
        request_gen_interval_min = int_value;
    } else if (key == "request_gen_interval_max") {
        request_gen_interval_max = int_value;
    } else if (key == "min_processing_time") {
        min_processing_time = int_value;
    } else if (key == "max_processing_time") {
        max_processing_time = int_value;
    } else {
        // Unknown key
        std::cerr << "Warning: Unknown config key: " << key << std::endl;
    }
    
    return true;
}