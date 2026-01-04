#include "LoadBalancer.h"
#include "Logger.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>

// Request generator thread function
void requestGenerator(LoadBalancer* lb, const Config& config, std::atomic<bool>& shutdown_flag, 
                      std::atomic<int>& request_counter, int lb_id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> interval_dist(
        config.request_gen_interval_min, 
        config.request_gen_interval_max
    );
    
    while (!shutdown_flag) {
        // Sleep for random interval
        int sleep_ms = interval_dist(gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        
        if (shutdown_flag) break;
        
        // Generate and add request
        int req_id = request_counter.fetch_add(1);
        Request req(req_id, 0, config.min_processing_time, config.max_processing_time);
        lb->addRequest(req);
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string config_file = "config.txt";
    if (argc > 1) {
        config_file = argv[1];
    }
    
    // Load configuration
    Config config;
    if (!config.loadFromFile(config_file)) {
        std::cerr << "Error: Failed to load configuration from " << config_file << std::endl;
        return 1;
    }
    
    std::cout << "=== Load Balancer ===" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "\tLoad Balancers: " << config.num_loadbalancers << std::endl;
    std::cout << "\tInitial Servers per LB: " << config.initial_servers << std::endl;
    std::cout << "\tRuntime Cycles: " << config.runtime_cycles << std::endl;
    std::cout << "\tScaling Thresholds: High=" << config.scaling_threshold_high 
              << ", Low=" << config.scaling_threshold_low << std::endl;
    std::cout << "\tServer Range: [" << config.min_servers << ", " 
              << config.max_servers << "]" << std::endl;
    std::cout << std::endl;
    
    // Create shared resources
    std::atomic<int> global_clock(0);
    Logger logger("loadbalancer.log");
    logger.setLogLevel(config.log_level);  // Set log verbosity level
    std::atomic<bool> shutdown_flag(false);
    std::atomic<int> request_counter(0);
    
    // Create LoadBalancers
    std::vector<std::unique_ptr<LoadBalancer>> loadbalancers;
    for (int i = 0; i < config.num_loadbalancers; i++) {
        loadbalancers.push_back(
            std::make_unique<LoadBalancer>(i, config, &global_clock, &logger)
        );
    }
    
    // Pre-fill each LoadBalancer queue with initial requests
    std::cout << "Pre-filling queues with initial requests..." << std::endl;
    int initial_requests_per_lb = config.initial_servers * 20;
    std::vector<size_t> initial_queue_sizes;
    for (int i = 0; i < config.num_loadbalancers; i++) {
        for (int j = 0; j < initial_requests_per_lb; j++) {
            int req_id = request_counter.fetch_add(1);
            Request req(req_id, 0, config.min_processing_time, config.max_processing_time);
            loadbalancers[i]->addRequest(req);
        }
        initial_queue_sizes.push_back(loadbalancers[i]->getQueueSize());
    }
    std::cout << "\tAdded " << initial_requests_per_lb << " requests to each LoadBalancer" 
              << std::endl << std::endl;
    
    // Spawn request generator threads
    std::vector<std::jthread> generator_threads;
    for (int i = 0; i < config.num_loadbalancers; i++) {
        generator_threads.emplace_back(
            requestGenerator, 
            loadbalancers[i].get(), 
            std::ref(config), 
            std::ref(shutdown_flag), 
            std::ref(request_counter),
            i
        );
    }
    
    std::cout << "Starting..." << std::endl << std::endl;
    
    // Main clock loop
    auto start_time = std::chrono::steady_clock::now();
    
    for (int cycle = 0; cycle < config.runtime_cycles; cycle++) {
        global_clock.store(cycle);
        
        // Check and scale servers every 100 cycles
        if (cycle % 100 == 0) {
            for (auto& lb : loadbalancers) {
                lb->checkAndScaleServers(cycle);
            }
        }
        
        // Log periodic statistics at configured intervals (PERIODIC mode only)
        if (cycle % config.log_interval == 0 && cycle > 0) {
            for (auto& lb : loadbalancers) {
                lb->logPeriodicStats(cycle);
            }
        }
        
        // Log statistics every 500 cycles (VERBOSE mode fallback for compatibility)
        if (cycle % 500 == 0 && cycle > 0 && logger.getLogLevel() == 0) {
            std::ostringstream oss;
            oss << "\n=== Cycle " << cycle << " Statistics ===\n";
            for (int i = 0; i < config.num_loadbalancers; i++) {
                oss << "  LB " << i << ": "
                    << "Queue=" << loadbalancers[i]->getQueueSize()
                    << ", Servers=" << loadbalancers[i]->getServerCount()
                    << ", Scaling Events=" << loadbalancers[i]->getScalingEventCount()
                    << "\n";
            }
            logger.log(oss.str());
        }
        
        // Small sleep to simulate cycle timing (1ms per cycle)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    // Signal shutdown to request generators
    std::cout << "\n=== Shutting Down ===" << std::endl;
    shutdown_flag.store(true);
    
    // Wait for generator threads to finish (jthread auto-joins on destruction)
    generator_threads.clear();
    
    // Shutdown all LoadBalancers
    std::cout << "Shutting down LoadBalancers..." << std::endl;
    for (auto& lb : loadbalancers) {
        lb->shutdown();
    }
    
    // Capture statistics before destroying LoadBalancers
    std::vector<size_t> ending_queue_sizes;
    std::vector<int> servers_added_counts;
    std::vector<int> servers_removed_counts;
    
    for (int i = 0; i < config.num_loadbalancers; i++) {
        ending_queue_sizes.push_back(loadbalancers[i]->getQueueSize());
        servers_added_counts.push_back(loadbalancers[i]->getScalingEventsUp());
        servers_removed_counts.push_back(loadbalancers[i]->getScalingEventsDown());
    }
    
    // Explicitly destroy LoadBalancers to ensure all worker threads complete cleanup
    // This forces all std::jthread objects to join before we print final statistics
    loadbalancers.clear();
    
    // Print final statistics
    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Runtime: " << duration.count() << " seconds" << std::endl;
    std::cout << "Total cycles: " << config.runtime_cycles << std::endl;
    std::cout << "Task time range: [" << config.min_processing_time << ", " 
              << config.max_processing_time << "] ms" << std::endl;
    std::cout << std::endl;
    
    // Per-LoadBalancer statistics
    std::cout << "LoadBalancer Statistics:" << std::endl;
    int total_servers_added = 0;
    int total_servers_removed = 0;
    
    for (int i = 0; i < config.num_loadbalancers; i++) {
        size_t starting_queue = initial_queue_sizes[i];
        size_t ending_queue = ending_queue_sizes[i];
        int servers_added = servers_added_counts[i];
        int servers_removed = servers_removed_counts[i];
        
        // Calculate total processed by summing all server processed counts
        // (We'll need to add this capability or estimate it)
        
        std::cout << "  LB " << i << ":" << std::endl;
        std::cout << "    Starting queue size: " << starting_queue << std::endl;
        std::cout << "    Ending queue size: " << ending_queue << std::endl;
        std::cout << "    Servers added: " << servers_added << std::endl;
        std::cout << "    Servers removed: " << servers_removed << std::endl;
        
        total_servers_added += servers_added;
        total_servers_removed += servers_removed;
    }
    
    std::cout << std::endl;
    std::cout << "Total requests generated: " << request_counter.load() << std::endl;
    std::cout << "Total scaling events: +" << total_servers_added 
              << " -" << total_servers_removed << std::endl;
    
    std::cout << "\n=== Finished ===" << std::endl;
    
    return 0;
}
