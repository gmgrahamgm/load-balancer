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
     * @param type Server type ('S'=streaming, 'P'=processing, 'A'=any)
     */
    WebServer(int sid, int lbid, RequestQueue* q, Logger* logger, std::atomic<int>* clk, int level, char type);
    
    /**
     * Constructor for 'A' type servers: Accepts two queues for round-robin processing
     * @param sid Server ID (unique within a LoadBalancer)
     * @param lbid LoadBalancer ID (for logging)
     * @param q1 Pointer to first RequestQueue (streaming)
     * @param q2 Pointer to second RequestQueue (processing)
     * @param logger Pointer to shared Logger
     * @param clk Pointer to shared global clock
     * @param level Log verbosity level (0=VERBOSE, 1=PERIODIC, 2=QUIET)
     * @param type Server type (must be 'A')
     */
    WebServer(int sid, int lbid, RequestQueue* q1, RequestQueue* q2, Logger* logger, std::atomic<int>* clk, int level, char type);
    
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
     * Get server type ('S', 'P', or 'A')
     */
    char getServerType() const;
    
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
    char server_type;  // 'S' for streaming, 'P' for processing, 'A' for any
    int log_level;
    std::atomic<bool> is_busy;
    std::atomic<int> total_requests_processed;
    std::atomic<int> processed_count_interval;  // Requests processed since last log
    RequestQueue* queue_ptr;
    RequestQueue* queue_ptr_secondary;  // For 'A' servers: second queue for round-robin
    std::atomic<bool> use_primary_queue;  // For 'A' servers: toggle between queues
    Logger* log_ptr;
    std::atomic<int>* clock_ptr;
    std::jthread worker_thread;  // Must be last - initialized after all members it references
};

#endif // WEBSERVER_H
