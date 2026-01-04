#ifndef REQUEST_H
#define REQUEST_H

#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstdint>

/**
 * Represents a web request with random IP addresses, processing time, and job type.
 * Each request is uniquely identified and timestamped.
 * Uses a random number generator from mt19937 and uniform_int_distribution, gathered from https://codeforces.com/blog/entry/61587?locale=ru.
 */
struct Request {
    std::string ip_in;        // Incoming IP address
    std::string ip_out;       // Outgoing IP address
    int processing_time;      // Clock cycles needed to process
    char job_type;            // 'S' for streaming, 'P' for processing
    int request_id;           // Unique identifier
    int arrival_time;         // Clock cycle when request was created
    
    /**
     * Generate a random IP address
     */
    static std::string generateRandomIP() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dist(0, 255);
        
        std::ostringstream oss;
        oss << dist(gen) << "." 
            << dist(gen) << "." 
            << dist(gen) << "." 
            << dist(gen);
        return oss.str();
    }
    
    /**
     * Convert dotted-decimal IP address to 32-bit unsigned integer
     * @param ip IP address string (e.g., "192.168.1.1")
     * @return 32-bit representation, or 0 if invalid format
     */
    static uint32_t ipToUint32(const std::string& ip) {
        std::istringstream iss(ip);
        std::string octet;
        uint32_t result = 0;
        int count = 0;
        
        while (std::getline(iss, octet, '.')) {
            if (count >= 4) return 0;  // Too many octets
            
            // Convert octet to integer
            int value;
            try {
                value = std::stoi(octet);
            } catch (...) {
                return 0;  // Invalid number
            }
            
            // Validate range
            if (value < 0 || value > 255) return 0;
            
            // Shift and add to result
            result = (result << 8) | static_cast<uint32_t>(value);
            count++;
        }
        
        // Must have exactly 4 octets
        if (count != 4) return 0;
        
        return result;
    }
    
    /**
     * Constructor: Creates request with random values
     * @param id Unique request identifier
     * @param cycle Current clock cycle (arrival time)
     * @param min_time Minimum processing time
     * @param max_time Maximum processing time
     */
    Request(int id = 0, int cycle = 0, int min_time = 10, int max_time = 100) 
        : request_id(id), arrival_time(cycle) {
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        // Generate random IPs
        ip_in = generateRandomIP();
        ip_out = generateRandomIP();
        
        // Random processing time
        std::uniform_int_distribution<int> time_dist(min_time, max_time);
        processing_time = time_dist(gen);
        
        // Random job type (50% S, 50% P)
        std::uniform_int_distribution<int> job_dist(0, 1);
        job_type = (job_dist(gen) == 0) ? 'S' : 'P';
    }
    
    /**
     * Display request information
     */
    std::string toString() const {
        std::ostringstream oss;
        oss << "Request[" << request_id << "] "
            << ip_in << " -> " << ip_out 
            << " (job: " << job_type 
            << ", time: " << processing_time 
            << ", arrival: " << arrival_time << ")";
        return oss.str();
    }
};

#endif // REQUEST_H
