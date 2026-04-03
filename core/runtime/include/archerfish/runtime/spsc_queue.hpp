#pragma once

#include <atomic>
#include <complex>
#include <optional>
#include <vector>

namespace archerfish::runtime {

template <typename T>
class SpscQueue {
public:
    explicit SpscQueue(size_t capacity)
        : buffer_(capacity + 1), capacity_(capacity + 1) {}

    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

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
