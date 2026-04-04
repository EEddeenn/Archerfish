#pragma once

#include <atomic>
#include <complex>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>
#include <vector>

namespace archerfish::runtime {

template <typename T>
class SpscQueue {
public:
    explicit SpscQueue(size_t capacity)
        : buffer_(capacity + 1), capacity_(capacity + 1) {}

    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    // --- Lock-free hot-path methods (unchanged) ---

    [[nodiscard]] bool push(T&& item) {
        auto tail = tail_.load(std::memory_order_relaxed);
        auto next_tail = (tail + 1) % capacity_;
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[tail] = std::move(item);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::optional<T> pop() {
        auto head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        auto item = std::move(buffer_[head]);
        head_.store((head + 1) % capacity_, std::memory_order_release);
        return item;
    }

    [[nodiscard]] bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool full() const {
        auto tail = tail_.load(std::memory_order_acquire);
        auto head = head_.load(std::memory_order_acquire);
        return ((tail + 1) % capacity_) == head;
    }

    [[nodiscard]] size_t size() const {
        auto tail = tail_.load(std::memory_order_acquire);
        auto head = head_.load(std::memory_order_acquire);
        return (tail + capacity_ - head) % capacity_;
    }

    [[nodiscard]] size_t capacity() const {
        return capacity_ - 1;
    }

    // --- Condition-variable augmented methods ---

    /// Lock-free push + notify one consumer waiting on pop_wait().
    [[nodiscard]] bool push_notify(T&& item) {
        bool ok = push(std::move(item));
        if (ok) {
            {
                std::lock_guard lock(cv_mutex_);
            }
            cv_not_empty_.notify_one();
        }
        return ok;
    }

    /// Blocking pop. Returns nullopt when stop is requested.
    std::optional<T> pop_wait(std::stop_token stoken) {
        while (true) {
            auto result = pop();
            if (result.has_value()) {
                {
                    std::lock_guard lock(cv_mutex_);
                }
                cv_not_full_.notify_one();
                return result;
            }

            std::unique_lock lock(cv_mutex_);
            if (stoken.stop_requested()) return std::nullopt;
            cv_not_empty_.wait(lock, [&] { return !empty() || stoken.stop_requested(); });
        }
    }

    /// Blocking push when queue is full. Returns false when stop is requested.
    bool push_wait(T&& item, std::stop_token stoken) {
        while (true) {
            if (push(std::move(item))) {
                {
                    std::lock_guard lock(cv_mutex_);
                }
                cv_not_empty_.notify_one();
                return true;
            }

            std::unique_lock lock(cv_mutex_);
            if (stoken.stop_requested()) return false;
            cv_not_full_.wait(lock, [&] { return !full() || stoken.stop_requested(); });
        }
    }

    /// Wake all threads waiting in pop_wait() or push_wait(). Call on shutdown.
    void notify_all() {
        std::lock_guard lock(cv_mutex_);
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }

    // --- CV access for external waits (e.g. priming) ---
    mutable std::mutex cv_mutex_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;

private:
    std::vector<T> buffer_;
    size_t capacity_;
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};

struct SampleBlock {
    std::vector<std::complex<float>> samples;
    bool start_of_burst{false};
    bool end_of_burst{false};
    double time_spec_sec{0.0};
    bool has_time_spec{false};
};

using SampleQueue = SpscQueue<SampleBlock>;

} // namespace archerfish::runtime
