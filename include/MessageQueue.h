#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include "Common.h"

template<typename T>
class MessageQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool shutdown_ = false;

public:
    // Push message to queue
    void push(const T& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(message);
        cv_.notify_one();
    }

    void push(T&& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(message));
        cv_.notify_one();
    }

    // Pop message from queue (blocking)
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
        
        if (shutdown_ && queue_.empty()) {
            return std::nullopt;
        }
        
        T message = std::move(queue_.front());
        queue_.pop();
        return message;
    }

    // Try to pop message with timeout
    std::optional<T> tryPop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || shutdown_; })) {
            return std::nullopt;
        }
        
        if (shutdown_ && queue_.empty()) {
            return std::nullopt;
        }
        
        T message = std::move(queue_.front());
        queue_.pop();
        return message;
    }

    // Check if queue is empty
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    // Get queue size
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    // Shutdown the queue
    void requestShutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        cv_.notify_all();
    }

    // Check if shutdown requested
    bool isShutdown() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutdown_;
    }
};

#endif // MESSAGE_QUEUE_H
