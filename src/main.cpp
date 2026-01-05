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
                      std::atomic<int>& request_counter, int lb_id, std::atomic<int>* global_clock) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> interval_dist(
        config.request_gen_interval_min, 
        config.request_gen_interval_max
    );
    
    while (!shutdown_flag) {
        // Calculate current position in simulation timeline
        int current_cycle = global_clock->load();
        double progress = (double)current_cycle / config.runtime_cycles;
        
        // Determine activity level based on simulation progress
        int sleep_ms;
        if (progress >= 0.6 && progress < 0.9) {
            // High activity period (60%-90%): burst mode
            sleep_ms = config.request_gen_interval_min;
        } else if (progress >= 0.2 && progress < 0.4) {
            // Low activity period (20%-40%): quiet period
            sleep_ms = config.request_gen_interval_max;
        } else {
            // Normal activity: random interval
            sleep_ms = interval_dist(gen);
        }

        // Set sleep duration if not using low and high period
        // sleep_ms = interval_dist(gen);
        
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
    
    // Create shared resources (logger needed for config output)
    std::atomic<int> global_clock(0);
    Logger logger("loadbalancer.log");
    logger.setLogLevel(config.log_level);  // Set log verbosity level
    std::atomic<bool> shutdown_flag(false);
    std::atomic<int> request_counter(0);
    
    // Log configuration to both console and file
    std::ostringstream config_oss;
    config_oss << "=== Load Balancer ===\n";
    config_oss << "Configuration:\n";
    config_oss << "\tLoad Balancers: " << config.num_loadbalancers << "\n";
    config_oss << "\tInitial Servers per LB: " << config.initial_servers << "\n";
    config_oss << "\tRuntime Cycles: " << config.runtime_cycles << "\n";
    config_oss << "\tScaling Thresholds: High=" << config.scaling_threshold_high 
              << ", Low=" << config.scaling_threshold_low << "\n";
    config_oss << "\tServer Range: [" << config.min_servers << ", " 
              << config.max_servers << "]";
    logger.log(config_oss.str(), LogColor::NONE);
    
    // Create LoadBalancers
    std::vector<std::unique_ptr<LoadBalancer>> loadbalancers;
    for (int i = 0; i < config.num_loadbalancers; i++) {
        loadbalancers.push_back(
            std::make_unique<LoadBalancer>(i, config, &global_clock, &logger)
        );
    }
    
    // Pre-fill each LoadBalancer queue with initial requests
    // ? Not sure if I should do initial requests, idk what Lightfoot wants when he says "print starting queue and ending queue"
    std::ostringstream prefill_oss;
    prefill_oss << "Pre-filling queues with initial requests...";
    logger.log(prefill_oss.str(), LogColor::NONE);
    
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
    
    std::ostringstream added_oss;
    added_oss << "\tAdded " << initial_requests_per_lb << " requests to each LoadBalancer";
    logger.log(added_oss.str(), LogColor::NONE);
    
    // Spawn request generator threads
    std::vector<std::jthread> generator_threads;
    for (int i = 0; i < config.num_loadbalancers; i++) {
        generator_threads.emplace_back(
            requestGenerator, 
            loadbalancers[i].get(), 
            std::ref(config), 
            std::ref(shutdown_flag), 
            std::ref(request_counter),
            i,
            &global_clock
        );
    }
    
    std::ostringstream starting_oss;
    starting_oss << "Starting...";
    logger.log(starting_oss.str(), LogColor::NONE);
    
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
    std::ostringstream shutdown_start_oss;
    shutdown_start_oss << "\n=== Shutting Down ===";
    logger.log(shutdown_start_oss.str(), LogColor::NONE);
    
    shutdown_flag.store(true);
    
    // Wait for generator threads to finish (jthread auto-joins on destruction)
    generator_threads.clear();
    
    // Shutdown all LoadBalancers
    std::ostringstream shutdown_lb_oss;
    shutdown_lb_oss << "Shutting down LoadBalancers...";
    logger.log(shutdown_lb_oss.str(), LogColor::NONE);
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
    
    // Log final statistics to both console and file
    std::ostringstream final_oss;
    final_oss << "\n=== Final Statistics ===\n";
    final_oss << "Runtime: " << duration.count() << " seconds\n";
    final_oss << "Total cycles: " << config.runtime_cycles << "\n";
    final_oss << "Task time range: [" << config.min_processing_time << ", " 
              << config.max_processing_time << "] ms\n";
    final_oss << "\n";
    
    // Per-LoadBalancer statistics
    final_oss << "LoadBalancer Statistics:\n";
    int total_servers_added = 0;
    int total_servers_removed = 0;
    
    for (int i = 0; i < config.num_loadbalancers; i++) {
        size_t starting_queue = initial_queue_sizes[i];
        size_t ending_queue = ending_queue_sizes[i];
        int servers_added = servers_added_counts[i];
        int servers_removed = servers_removed_counts[i];
        
        // Calculate total processed by summing all server processed counts
        // (We'll need to add this capability or estimate it)
        
        final_oss << "  LB " << i << ":\n";
        final_oss << "    Starting queue size: " << starting_queue << "\n";
        final_oss << "    Ending queue size: " << ending_queue << "\n";
        final_oss << "    Servers added: " << servers_added << "\n";
        final_oss << "    Servers removed: " << servers_removed << "\n";
        
        total_servers_added += servers_added;
        total_servers_removed += servers_removed;
    }
    
    final_oss << "\n";
    final_oss << "Total requests generated: " << request_counter.load() << "\n";
    final_oss << "Total scaling events: +" << total_servers_added 
              << " -" << total_servers_removed << "\n";
    final_oss << "\n=== Finished ===";
    
    logger.log(final_oss.str(), LogColor::NONE);
    
    return 0;
}
