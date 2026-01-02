#include "LoadBalancer.h"
#include <iostream>
#include <sstream>

LoadBalancer::LoadBalancer(int id, const Config& cfg, std::atomic<int>* clk, Logger* logger)
    : lb_id(id), config(cfg), last_scaling_cycle(-cfg.scaling_cooldown),
      scaling_events_up(0), scaling_events_down(0), next_server_id(0),
      global_clock_ptr(clk), log_ptr(logger), is_shutdown(false) {
    
    // Initialize IP blocker with configured ranges
    if (!config.blocked_ip_ranges.empty()) {
        int loaded = ip_blocker.loadBlockedRanges(config.blocked_ip_ranges);
        std::ostringstream oss;
        oss << "[LB:" << lb_id << "] Loaded " << loaded << " blocked IP ranges";
        log_ptr->log(oss.str());
    }
    
    // Create initial server pool
    for (int i = 0; i < config.initial_servers; i++) {
        addServer();
    }
    
    // Log initialization
    std::ostringstream oss;
    oss << "[LB:" << lb_id << "][Cycle:" << global_clock_ptr->load() << "] "
        << "Initialized with " << config.initial_servers << " servers";
    log_ptr->log(oss.str());
}

LoadBalancer::~LoadBalancer() {
    if (!is_shutdown) {
        shutdown();
    }
}

void LoadBalancer::addRequest(const Request& req) {
    // Check if either IP, in or out, is blocked
    if (ip_blocker.isBlocked(req.ip_in)) {
        std::ostringstream oss;
        oss << "[LB:" << lb_id << "][BLOCKED] Request " << req.request_id 
            << " - incoming IP " << req.ip_in << " is blocked";
        log_ptr->log(oss.str());
        return;
    }
    
    if (ip_blocker.isBlocked(req.ip_out)) {
        std::ostringstream oss;
        oss << "[LB:" << lb_id << "][BLOCKED] Request " << req.request_id 
            << " - outgoing IP " << req.ip_out << " is blocked";
        log_ptr->log(oss.str());
        return;
    }
    
    queue.push(req);
}

void LoadBalancer::checkAndScaleServers(int current_cycle) {
    // Check if enough time has passed since last scaling
    if ((current_cycle - last_scaling_cycle) < config.scaling_cooldown) {
        return;
    }
    
    size_t queue_size = queue.size();
    int current_servers = getServerCount();
    
    // Scale up if queue is too large
    if (queue_size > static_cast<size_t>(config.scaling_threshold_high * current_servers)) {
        if (current_servers < config.max_servers) {
            addServer();
            last_scaling_cycle = current_cycle;
            scaling_events_up++;
            
            std::ostringstream oss;
            oss << "[LB:" << lb_id << "][Cycle:" << current_cycle << "] "
                << "Scaled UP to " << getServerCount() << " servers "
                << "(queue size: " << queue_size << ")";
            log_ptr->log(oss.str());
        }
    }
    // Scale down if queue is too small
    else if (queue_size < static_cast<size_t>(config.scaling_threshold_low * current_servers)) {
        if (current_servers > config.min_servers) {
            removeServer();
            last_scaling_cycle = current_cycle;
            scaling_events_down++;
            
            std::ostringstream oss;
            oss << "[LB:" << lb_id << "][Cycle:" << current_cycle << "] "
                << "Scaled DOWN to " << getServerCount() << " servers "
                << "(queue size: " << queue_size << ")";
            log_ptr->log(oss.str());
        }
    }
}

void LoadBalancer::shutdown() {
    std::ostringstream oss;
    oss << "[LB:" << lb_id << "][Cycle:" << global_clock_ptr->load() << "] "
        << "Shutting down...";
    log_ptr->log(oss.str());
    
    // Signal queue shutdown to release all blocking servers
    queue.setShutdown();
    
    // jthread will automatically join when webservers are destroyed
    // No need to manually join
    
    // Log final statistics
    int total_processed = 0;
    {
        std::lock_guard<std::mutex> lock(server_vector_mutex);
        for (const auto& server : webservers) {
            total_processed += server->getProcessedCount();
        }
    }
    
    std::ostringstream oss2;
    oss2 << "[LB:" << lb_id << "] Shutdown complete. "
         << "Total requests processed: " << total_processed
         << ", Scaling events: +" << scaling_events_up 
         << " -" << scaling_events_down;
    log_ptr->log(oss2.str());
    
    is_shutdown = true;
}

size_t LoadBalancer::getQueueSize() const {
    return queue.size();
}

int LoadBalancer::getServerCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(server_vector_mutex));
    return webservers.size();
}

int LoadBalancer::getScalingEventCount() const {
    return scaling_events_up + scaling_events_down;
}

int LoadBalancer::getLoadBalancerId() const {
    return lb_id;
}

void LoadBalancer::addServer() {
    std::lock_guard<std::mutex> lock(server_vector_mutex);
    int sid = next_server_id++;
    webservers.emplace_back(
        std::make_unique<WebServer>(sid, lb_id, &queue, log_ptr, global_clock_ptr)
    );
}

void LoadBalancer::removeServer() {
    std::unique_ptr<WebServer> server_to_remove;
    
    {
        std::lock_guard<std::mutex> lock(server_vector_mutex);
        
        if (webservers.empty()) {
            return;
        }
        
        // Request shutdown of the last server
        webservers.back()->requestShutdown();
        
        // Move server out of vector
        server_to_remove = std::move(webservers.back());
        webservers.pop_back();
    }
    
    // jthread will automatically join when server_to_remove goes out of scope
    // No need to manually call join()
}
