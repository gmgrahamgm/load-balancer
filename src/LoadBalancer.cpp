#include "LoadBalancer.h"
#include <iostream>
#include <sstream>

LoadBalancer::LoadBalancer(int id, const Config& cfg, std::atomic<int>* clk, Logger* logger)
    : lb_id(id), config(cfg), last_scaling_cycle(-cfg.scaling_cooldown),
      last_log_cycle(0), scaling_events_up(0), scaling_events_down(0), next_server_id(0),
      global_clock_ptr(clk), log_ptr(logger), is_shutdown(false) {
    
    // Initialize IP blocker with configured ranges
    if (!config.blocked_ip_ranges.empty()) {
        int loaded = ip_blocker.loadBlockedRanges(config.blocked_ip_ranges);
        std::ostringstream oss;
        oss << "[LB:" << lb_id << "] Loaded " << loaded << " blocked IP ranges";
        log_ptr->log(oss.str());
    }
    
    // Create initial server pool based on type percentages
    int num_streaming = static_cast<int>(config.initial_servers * config.streaming_servers);
    int num_processing = static_cast<int>(config.initial_servers * config.processing_servers);
    int num_any = config.initial_servers - num_streaming - num_processing;
    
    for (int i = 0; i < num_streaming; i++) {
        addServer('S');
    }
    for (int i = 0; i < num_processing; i++) {
        addServer('P');
    }
    for (int i = 0; i < num_any; i++) {
        addServer('A');
    }
    
    // Log initialization
    std::ostringstream oss;
    oss << "[LB:" << lb_id << "][Cycle:" << global_clock_ptr->load() << "] "
        << "Initialized with " << config.initial_servers << " servers "
        << "(S:" << num_streaming << ", P:" << num_processing << ", A:" << num_any << ")";
    log_ptr->log(oss.str());
}

LoadBalancer::~LoadBalancer() {
    if (!is_shutdown) {
        shutdown();
    }
}

void LoadBalancer::addRequest(const Request& req) {
    // Check if either IP (in or out) is blocked
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
    
    // Route request based on sorting configuration
    if (config.sorting) {
        // Type-based routing: S requests to streaming queue, P to processing queue
        if (req.job_type == 'S') {
            queue_streaming.push(req);
        } else if (req.job_type == 'P') {
            queue_processing.push(req);
        } else {
            // Unknown job type - default to streaming
            queue_streaming.push(req);
        }
    } else {
        // No sorting: push to both queues (servers will compete for requests)
        queue_streaming.push(req);
        queue_processing.push(req);
    }
}

void LoadBalancer::checkAndScaleServers(int current_cycle) {
    // Check if enough time has passed since last scaling
    if ((current_cycle - last_scaling_cycle) < config.scaling_cooldown) {
        return;
    }
    
    size_t queue_size = queue_streaming.size() + queue_processing.size();
    int current_servers = getServerCount();
    
    // Scale up if queue is too large
    if (queue_size > static_cast<size_t>(config.scaling_threshold_high * current_servers)) {
        if (current_servers < config.max_servers) {
            // Add server based on which queue is larger (or 'A' if similar)
            size_t s_size = queue_streaming.size();
            size_t p_size = queue_processing.size();
            char type = (s_size > p_size * 1.5) ? 'S' : (p_size > s_size * 1.5) ? 'P' : 'A';
            addServer(type);
            last_scaling_cycle = current_cycle;
            scaling_events_up++;
            
            std::ostringstream oss;
            oss << "[LB:" << lb_id << "][Cycle:" << current_cycle << "] "
                << "Scaled UP to " << getServerCount() << " servers "
                << "(queue size: S=" << s_size << ", P=" << p_size << ")";
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
    
    // Signal both queues to shutdown and release all blocking servers
    queue_streaming.setShutdown();
    queue_processing.setShutdown();
    
    // jthread will automatically join when webservers are destroyed
    // No need to manually join
    
    // Log final statistics
    int total_processed = 0;
    {
        std::lock_guard<std::mutex> lock(server_vector_mutex);
        for (const auto& server : webservers_streaming) {
            total_processed += server->getProcessedCount();
        }
        for (const auto& server : webservers_processing) {
            total_processed += server->getProcessedCount();
        }
        for (const auto& server : webservers_any) {
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
    return queue_streaming.size() + queue_processing.size();
}

int LoadBalancer::getServerCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(server_vector_mutex));
    return webservers_streaming.size() + webservers_processing.size() + webservers_any.size();
}

int LoadBalancer::getScalingEventCount() const {
    return scaling_events_up + scaling_events_down;
}

int LoadBalancer::getScalingEventsUp() const {
    return scaling_events_up;
}

int LoadBalancer::getScalingEventsDown() const {
    return scaling_events_down;
}

int LoadBalancer::getLoadBalancerId() const {
    return lb_id;
}

void LoadBalancer::logPeriodicStats(int current_cycle) {
    // Only log in PERIODIC mode (level 1)
    if (log_ptr->getLogLevel() != 1) {
        return;
    }
    
    last_log_cycle = current_cycle;
    
    // Gather stats from all WebServers
    std::lock_guard<std::mutex> lock(server_vector_mutex);
    
    int total_processed_interval = 0;
    for (const auto& server : webservers_streaming) {
        total_processed_interval += server->getProcessedSinceLastLog();
    }
    for (const auto& server : webservers_processing) {
        total_processed_interval += server->getProcessedSinceLastLog();
    }
    for (const auto& server : webservers_any) {
        total_processed_interval += server->getProcessedSinceLastLog();
    }
    
    // Log the statistics
    std::ostringstream oss;
    oss << "\n=== Cycle " << current_cycle << " Statistics ===\n";
    oss << "  LB " << lb_id << ": ";
    oss << "QueueS=" << queue_streaming.size() << ", ";
    oss << "QueueP=" << queue_processing.size() << ", ";
    oss << "Servers=" << (webservers_streaming.size() + webservers_processing.size() + webservers_any.size()) << " (S:" << webservers_streaming.size() << ",P:" << webservers_processing.size() << ",A:" << webservers_any.size() << "), ";
    oss << "Processed=" << total_processed_interval << ", ";
    oss << "Scaling Events=" << (scaling_events_up.load() + scaling_events_down.load());
    log_ptr->log(oss.str());
    
    // Reset interval counters
    for (const auto& server : webservers_streaming) {
        server->resetProcessedSinceLastLog();
    }
    for (const auto& server : webservers_processing) {
        server->resetProcessedSinceLastLog();
    }
    for (const auto& server : webservers_any) {
        server->resetProcessedSinceLastLog();
    }
}

void LoadBalancer::addServer(char type) {
    std::lock_guard<std::mutex> lock(server_vector_mutex);
    int sid = next_server_id++;
    
    if (type == 'S') {
        webservers_streaming.emplace_back(
            std::make_unique<WebServer>(sid, lb_id, &queue_streaming, log_ptr, global_clock_ptr, log_ptr->getLogLevel(), 'S')
        );
    } else if (type == 'P') {
        webservers_processing.emplace_back(
            std::make_unique<WebServer>(sid, lb_id, &queue_processing, log_ptr, global_clock_ptr, log_ptr->getLogLevel(), 'P')
        );
    } else { // type == 'A'
        // 'A' servers pull from both queues in round-robin fashion
        webservers_any.emplace_back(
            std::make_unique<WebServer>(sid, lb_id, &queue_streaming, &queue_processing, log_ptr, global_clock_ptr, log_ptr->getLogLevel(), 'A')
        );
    }
}

void LoadBalancer::removeServer() {
    std::unique_ptr<WebServer> server_to_remove;
    
    {
        std::lock_guard<std::mutex> lock(server_vector_mutex);
        
        // Remove from the largest pool first
        if (webservers_any.size() > 0) {
            webservers_any.back()->requestShutdown();
            server_to_remove = std::move(webservers_any.back());
            webservers_any.pop_back();
        } else if (webservers_streaming.size() >= webservers_processing.size() && webservers_streaming.size() > 0) {
            webservers_streaming.back()->requestShutdown();
            server_to_remove = std::move(webservers_streaming.back());
            webservers_streaming.pop_back();
        } else if (webservers_processing.size() > 0) {
            webservers_processing.back()->requestShutdown();
            server_to_remove = std::move(webservers_processing.back());
            webservers_processing.pop_back();
        } else {
            return; // No servers to remove
        }
    }
    
    // jthread will automatically join when server_to_remove goes out of scope
    // No need to manually call join()
}
