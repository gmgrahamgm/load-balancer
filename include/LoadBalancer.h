#ifndef LOADBALANCER_H
#define LOADBALANCER_H

#include "Config.h"
#include "WebServer.h"
#include "RequestQueue.h"
#include "Logger.h"
#include "IPBlocker.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

/**
 * LoadBalancer orchestrates WebServers and manages dynamic scaling.
 * Monitors queue size and scales server count based on thresholds.
 */
class LoadBalancer {
public:
    /**
     * Constructor: Creates LoadBalancer with initial server pool
     * @param id LoadBalancer ID (for multi-LB support)
     * @param cfg Configuration parameters
     * @param clk Pointer to shared global clock
     * @param logger Pointer to shared Logger
     */
    LoadBalancer(int id, const Config& cfg, std::atomic<int>* clk, Logger* logger);
    
    /**
     * Destructor: Ensures clean shutdown
     */
    ~LoadBalancer();
    
    /**
     * Add request to the queue (called by main/generator thread)
     */
    void addRequest(const Request& req);
    
    /**
     * Check queue size and scale servers up/down if needed
     * Called periodically by main driver
     * @param current_cycle Current clock cycle for cooldown tracking
     */
    void checkAndScaleServers(int current_cycle);
    
    /**
     * Shutdown all servers and wait for completion
     */
    void shutdown();
    
    /**
     * Get current queue size
     */
    size_t getQueueSize() const;
    
    /**
     * Get current number of active servers
     */
    int getServerCount() const;
    
    /**
     * Get total scaling events (up + down)
     */
    int getScalingEventCount() const;
    
    /**
     * Get number of scale-up events
     */
    int getScalingEventsUp() const;
    
    /**
     * Get number of scale-down events
     */
    int getScalingEventsDown() const;
    
    /**
     * Get LoadBalancer ID
     */
    int getLoadBalancerId() const;
    
    /**
     * Log periodic statistics (called at log_interval)
     * @param current_cycle Current clock cycle
     */
    void logPeriodicStats(int current_cycle);
    
private:
    /**
     * Add a new server to the pool
     * @param type Server type ('S', 'P', or 'A')
     */
    void addServer(char type);
    
    /**
     * Remove a server from the pool (graceful shutdown)
     */
    void removeServer();
    
    int lb_id;
    Config config;
    RequestQueue queue_streaming;   // Queue for streaming requests
    RequestQueue queue_processing;  // Queue for processing requests
    std::vector<std::unique_ptr<WebServer>> webservers_streaming;
    std::vector<std::unique_ptr<WebServer>> webservers_processing;
    std::vector<std::unique_ptr<WebServer>> webservers_any;
    std::mutex server_vector_mutex;
    
    int last_scaling_cycle;
    int last_log_cycle;
    std::atomic<int> scaling_events_up;
    std::atomic<int> scaling_events_down;
    std::atomic<int> next_server_id;
    
    std::atomic<int>* global_clock_ptr;
    Logger* log_ptr;
    IPBlocker ip_blocker;
    
    bool is_shutdown;
};

#endif // LOADBALANCER_H
