#ifndef REQUEST_H
#define REQUEST_H

#include <string>
#include <random>
#include <sstream>
#include <iomanip>

/**
 * Represents a web request with random IP addresses, processing time, and job type.
 * Each request is uniquely identified and timestamped.
 * Uses a random number generator frim mt19937 and uniform_int_distribution, gathered from https://codeforces.com/blog/entry/61587?locale=ru.
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
     * Constructor: Creates request with random values
     * @param id Unique request identifier
     * @param cycle Current clock cycle (arrival time)
     * @param min_time Minimum processing time
     * @param max_time Maximum processing time
     */
    Request(int id, int cycle, int min_time, int max_time) 
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
