#include "WebServer.h"
#include <iostream>
#include <iomanip>
#include <sstream>

WebServer::WebServer(int sid, int lbid, RequestQueue* q, Logger* logger, std::atomic<int>* clk, int level, char type)
    : server_id(sid), lb_id(lbid), server_type(type), log_level(level), is_busy(false), 
      total_requests_processed(0), processed_count_interval(0),
      queue_ptr(q), queue_ptr_secondary(nullptr), use_primary_queue(true),
      log_ptr(logger), clock_ptr(clk),
      worker_thread([this](std::stop_token st) { workerThread(st); }) {
}

WebServer::WebServer(int sid, int lbid, RequestQueue* q1, RequestQueue* q2, Logger* logger, std::atomic<int>* clk, int level, char type)
    : server_id(sid), lb_id(lbid), server_type(type), log_level(level), is_busy(false), 
      total_requests_processed(0), processed_count_interval(0),
      queue_ptr(q1), queue_ptr_secondary(q2), use_primary_queue(true),
      log_ptr(logger), clock_ptr(clk),
      worker_thread([this](std::stop_token st) { workerThread(st); }) {
}

int WebServer::getProcessedCount() const {
    return total_requests_processed.load(std::memory_order_relaxed);
}

int WebServer::getProcessedSinceLastLog() const {
    return processed_count_interval.load(std::memory_order_relaxed);
}

void WebServer::resetProcessedSinceLastLog() {
    processed_count_interval.store(0, std::memory_order_relaxed);
}

bool WebServer::isBusy() const {
    return is_busy.load(std::memory_order_relaxed);
}

int WebServer::getServerId() const {
    return server_id;
}

char WebServer::getServerType() const {
    return server_type;
}

void WebServer::requestShutdown() {
    worker_thread.request_stop();
}

void WebServer::workerThread(std::stop_token st) {
    for (;;) {
        // Check if we're asked to stop before taking a new request
        if (st.stop_requested()) {
            break;
        }
        
        Request req;
        bool got_request = false;
        
        // 'A' servers use round-robin between two queues
        if (queue_ptr_secondary != nullptr && server_type == 'A') {
            // Try primary queue first based on toggle
            if (use_primary_queue.load()) {
                got_request = queue_ptr->pop(req, st);
                if (!got_request && !st.stop_requested()) {
                    // Primary queue empty or shutdown, try secondary
                    got_request = queue_ptr_secondary->pop(req, st);
                }
            } else {
                // Try secondary queue first
                got_request = queue_ptr_secondary->pop(req, st);
                if (!got_request && !st.stop_requested()) {
                    // Secondary queue empty or shutdown, try primary
                    got_request = queue_ptr->pop(req, st);
                }
            }
            
            // Toggle for next iteration
            use_primary_queue.store(!use_primary_queue.load());
            
            if (!got_request) {
                // Both queues returned false (likely shutdown)
                break;
            }
        } else {
            // Standard single-queue behavior for 'S' and 'P' servers
            if (!queue_ptr->pop(req, st)) {
                // Returns false on stop request or global queue shutdown
                break;
            }
        }
        
        // Mark as busy
        is_busy.store(true, std::memory_order_relaxed);
        
        // Log start of processing (only in VERBOSE mode)
        if (log_level == 0) {
            std::ostringstream oss;
            oss << "[LB:" << lb_id << "][Server:" << server_id 
                << "(" << server_type << ")" 
                << "][Cycle:" << clock_ptr->load() << "] "
                << "Processing request " << req.request_id 
                << " from " << req.ip_in 
                << " (job: " << req.job_type 
                << ", time: " << req.processing_time << ")";
            log_ptr->log(oss.str(), LogColor::WHITE);
        }
        
        // Simulate processing by sleeping
        std::this_thread::sleep_for(std::chrono::milliseconds(req.processing_time));
        
        // Log completion (only in VERBOSE mode)
        if (log_level == 0) {
            std::ostringstream oss2;
            oss2 << "[LB:" << lb_id << "][Server:" << server_id 
                 << "(" << server_type << ")" 
                 << "][Cycle:" << clock_ptr->load() << "] "
                 << "Completed request " << req.request_id;
            log_ptr->log(oss2.str(), LogColor::WHITE);
        }
        
        // Update stats and mark as idle
        total_requests_processed.fetch_add(1, std::memory_order_relaxed);
        processed_count_interval.fetch_add(1, std::memory_order_relaxed);
        is_busy.store(false, std::memory_order_relaxed);
        
        // If stop is requested during processing, we finish the request
        // and exit before taking the next one (checked at loop top)
    }
    
    // Log thread exit
    std::ostringstream oss;
    oss << "[LB:" << lb_id << "][Server:" << server_id << "(" << server_type << ")" << "] "
        << "Worker thread exiting. Total processed: " 
        << total_requests_processed.load(std::memory_order_relaxed);
    log_ptr->log(oss.str(), LogColor::NONE);
}
