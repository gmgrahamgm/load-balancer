#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "RequestQueue.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

/**
 * WebServer worker thread that processes requests from a queue.
 */
class WebServer {
public:
    /**
     * Constructor: Creates and starts a WebServer worker thread
     * @param sid Server ID (unique within a LoadBalancer)
     * @param lbid LoadBalancer ID (for logging)
     * @param q Pointer to shared RequestQueue
     * @param log_mtx Pointer to shared logging mutex
     * @param clk Pointer to shared global clock
     */
    WebServer(int sid, int lbid, RequestQueue* q, std::mutex* log_mtx, std::atomic<int>* clk);
    
    /**
     * Destructor
     */
    ~WebServer();
    
    /**
     * Wait for worker thread to complete
     */
    void join();
    
    /**
     * Get total requests processed by this server
     */
    int getProcessedCount() const;
    
    /**
     * Check if server is currently processing a request
     */
    bool isBusy() const;
    
    /**
     * Get server ID
     */
    int getServerId() const;
    
    /**
     * Request this server to gracefully shutdown after current request
     */
    void requestShutdown();
    
private:
    /**
     * Worker thread main loop - processes requests from queue
     */
    void workerThread();
    
    int server_id;
    int lb_id;
    std::atomic<bool> is_busy;
    std::atomic<bool> server_shutdown_flag;  // Individual server shutdown control
    std::atomic<int> total_requests_processed;
    std::thread worker_thread;
    RequestQueue* queue_ptr;
    std::mutex* log_mutex_ptr;
    std::atomic<int>* clock_ptr;
};

#endif // WEBSERVER_H
