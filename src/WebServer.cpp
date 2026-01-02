#include "WebServer.h"
#include <iostream>
#include <iomanip>

WebServer::WebServer(int sid, int lbid, RequestQueue* q, std::mutex* log_mtx, std::atomic<int>* clk)
    : server_id(sid), lb_id(lbid), is_busy(false), server_shutdown_flag(false),
      total_requests_processed(0), queue_ptr(q), log_mutex_ptr(log_mtx), clock_ptr(clk) {
    
    // Spawn worker thread
    worker_thread = std::thread(&WebServer::workerThread, this);
}

WebServer::~WebServer() {
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

void WebServer::join() {
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

int WebServer::getProcessedCount() const {
    return total_requests_processed;
}

bool WebServer::isBusy() const {
    return is_busy;
}

int WebServer::getServerId() const {
    return server_id;
}

void WebServer::requestShutdown() {
    server_shutdown_flag = true;
}

void WebServer::workerThread() {
    while (!server_shutdown_flag) {
        Request req;
        
        // Try to pop request from queue (blocking)
        if (!queue_ptr->pop(req)) {
            // Global queue shutdown - exit thread
            break;
        }
        
        // Check shutdown flag again after pop (in case it was set while blocking)
        if (server_shutdown_flag) {
            // Put request back if we need to shutdown
            queue_ptr->push(req);
            break;
        }
        
        // Mark as busy
        is_busy = true;
        
        // Log start of processing
        {
            std::lock_guard<std::mutex> lock(*log_mutex_ptr);
            std::cout << "[LB:" << lb_id << "][Server:" << server_id 
                      << "][Cycle:" << clock_ptr->load() << "] "
                      << "Processing request " << req.request_id 
                      << " from " << req.ip_in 
                      << " (job: " << req.job_type 
                      << ", time: " << req.processing_time << ")" << std::endl;
        }
        
        // Simulate processing by sleeping
        std::this_thread::sleep_for(std::chrono::milliseconds(req.processing_time));
        
        // Log completion
        {
            std::lock_guard<std::mutex> lock(*log_mutex_ptr);
            std::cout << "[LB:" << lb_id << "][Server:" << server_id 
                      << "][Cycle:" << clock_ptr->load() << "] "
                      << "Completed request " << req.request_id << std::endl;
        }
        
        // Update stats and mark as idle
        total_requests_processed++;
        is_busy = false;
    }
    
    // Log thread exit
    {
        std::lock_guard<std::mutex> lock(*log_mutex_ptr);
        std::cout << "[LB:" << lb_id << "][Server:" << server_id << "] "
                  << "Worker thread exiting. Total processed: " 
                  << total_requests_processed << std::endl;
    }
}
