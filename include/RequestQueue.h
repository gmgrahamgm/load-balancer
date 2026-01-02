#ifndef REQUESTQUEUE_H
#define REQUESTQUEUE_H

#include "Request.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

/**
 * Thread-safe queue for Request objects.
 * Implements producer-consumer pattern with blocking pop operation.
 */
class RequestQueue {
public:
    RequestQueue();
    ~RequestQueue();
    
    /**
     * Add request to queue (thread-safe).
     * Notifies one waiting consumer thread.
     */
    void push(const Request& req);
    
    /**
     * Remove and return request from queue (blocking, thread-safe).
     * Blocks if queue is empty until item available or shutdown.
     * @param req Reference to store the popped request
     * @return true if request retrieved, false if shutdown with empty queue
     */
    bool pop(Request& req);
    
    /**
     * Get current queue size.
     */
    size_t size() const;
    
    /**
     * Check if queue is empty.
     */
    bool isEmpty() const;
    
    /**
     * Signal shutdown to all waiting threads.
     * After calling this, pop() will return false when queue is empty.
     */
    void setShutdown();
    
private:
    std::queue<Request> requests;
    mutable std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::atomic<bool> shutdown_flag;
};

#endif // REQUESTQUEUE_H
