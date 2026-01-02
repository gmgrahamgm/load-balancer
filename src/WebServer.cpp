#include "WebServer.h"
#include <iostream>
#include <iomanip>
#include <sstream>

WebServer::WebServer(int sid, int lbid, RequestQueue* q, Logger* logger, std::atomic<int>* clk)
    : server_id(sid), lb_id(lbid), is_busy(false), total_requests_processed(0),
      queue_ptr(q), log_ptr(logger), clock_ptr(clk),
      worker_thread([this](std::stop_token st) { workerThread(st); }) {
}

int WebServer::getProcessedCount() const {
    return total_requests_processed.load(std::memory_order_relaxed);
}

bool WebServer::isBusy() const {
    return is_busy.load(std::memory_order_relaxed);
}

int WebServer::getServerId() const {
    return server_id;
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
        
        // Try to pop request from queue with stop_token support
        if (!queue_ptr->pop(req, st)) {
            // Returns false on stop request or global queue shutdown
            break;
        }
        
        // Mark as busy
        is_busy.store(true, std::memory_order_relaxed);
        
        // Log start of processing
        std::ostringstream oss;
        oss << "[LB:" << lb_id << "][Server:" << server_id 
            << "][Cycle:" << clock_ptr->load() << "] "
            << "Processing request " << req.request_id 
            << " from " << req.ip_in 
            << " (job: " << req.job_type 
            << ", time: " << req.processing_time << ")";
        log_ptr->log(oss.str());
        
        // Simulate processing by sleeping
        std::this_thread::sleep_for(std::chrono::milliseconds(req.processing_time));
        
        // Log completion
        std::ostringstream oss2;
        oss2 << "[LB:" << lb_id << "][Server:" << server_id 
             << "][Cycle:" << clock_ptr->load() << "] "
             << "Completed request " << req.request_id;
        log_ptr->log(oss2.str());
        
        // Update stats and mark as idle
        total_requests_processed.fetch_add(1, std::memory_order_relaxed);
        is_busy.store(false, std::memory_order_relaxed);
        
        // If stop is requested during processing, we finish the request
        // and exit before taking the next one (checked at loop top)
    }
    
    // Log thread exit
    std::ostringstream oss;
    oss << "[LB:" << lb_id << "][Server:" << server_id << "] "
        << "Worker thread exiting. Total processed: " 
        << total_requests_processed.load(std::memory_order_relaxed);
    log_ptr->log(oss.str());
}
