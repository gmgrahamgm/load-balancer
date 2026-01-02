#include "RequestQueue.h"

RequestQueue::RequestQueue() : shutdown_flag(false) {
}

RequestQueue::~RequestQueue() {
    setShutdown();
}

void RequestQueue::push(const Request& req) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    requests.push(req);
    queue_cv.notify_one();  // Wake up one waiting consumer
}

bool RequestQueue::pop(Request& req) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    
    // Wait while queue is empty AND not shutdown
    while (requests.empty() && !shutdown_flag) {
        queue_cv.wait(lock);
    }
    
    // If shutdown and queue is empty, return false
    if (requests.empty() && shutdown_flag) {
        return false;
    }
    
    // Pop request from queue
    req = requests.front();
    requests.pop();
    return true;
}

bool RequestQueue::pop(Request& req, std::stop_token st) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    
    // Wait until: queue has items OR shutdown OR stop requested
    queue_cv.wait(lock, st, [this] {
        return !requests.empty() || shutdown_flag;
    });
    
    // If stop was requested, exit
    if (st.stop_requested()) {
        return false;
    }
    
    // If shutdown and queue is empty, return false
    if (requests.empty() && shutdown_flag) {
        return false;
    }
    
    // Pop request from queue
    req = requests.front();
    requests.pop();
    return true;
}

size_t RequestQueue::size() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return requests.size();
}

bool RequestQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return requests.empty();
}

void RequestQueue::setShutdown() {
    shutdown_flag = true;
    queue_cv.notify_all();  // Wake up all waiting threads
}
