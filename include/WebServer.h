#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "RequestQueue.h"
#include "Logger.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <stop_token>

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
     * @param logger Pointer to shared Logger
     * @param clk Pointer to shared global clock
     * @param level Log verbosity level (0=VERBOSE, 1=PERIODIC, 2=QUIET)
     */
    WebServer(int sid, int lbid, RequestQueue* q, Logger* logger, std::atomic<int>* clk, int level);
    
    /**
     * Destructor - automatically joins worker thread
     */
    ~WebServer() = default;
    
    /**
     * Get total requests processed by this server
     */
    int getProcessedCount() const;
    
    /**
     * Get requests processed since last log interval
     */
    int getProcessedSinceLastLog() const;
    
    /**
     * Reset the interval counter (called after periodic stats)
     */
    void resetProcessedSinceLastLog();
    
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
     * @param st Stop token for graceful shutdown
     */
    void workerThread(std::stop_token st);
    
    int server_id;
    int lb_id;
    int log_level;
    std::atomic<bool> is_busy;
    std::atomic<int> total_requests_processed;
    std::atomic<int> processed_count_interval;  // Requests processed since last log
    RequestQueue* queue_ptr;
    Logger* log_ptr;
    std::atomic<int>* clock_ptr;
    std::jthread worker_thread;  // Must be last - initialized after all members it references
};

#endif // WEBSERVER_H
