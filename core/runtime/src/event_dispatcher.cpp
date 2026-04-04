#include "archerfish/runtime/event_dispatcher.hpp"

#include <algorithm>

namespace archerfish::runtime {

void EventDispatcher::schedule(double time_sec, std::function<void()> callback) {
    events_.push_back({time_sec, std::move(callback), false});
}

void EventDispatcher::start() {
    std::sort(events_.begin(), events_.end(),
              [](const ScheduledEvent& a, const ScheduledEvent& b) {
                  return a.time_sec < b.time_sec;
              });

    running_.store(true, std::memory_order_release);
    epoch_ = std::chrono::steady_clock::now();
    thread_ = std::thread(&EventDispatcher::run, this);
}

void EventDispatcher::wait_complete() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void EventDispatcher::cancel() {
    cancelled_.store(true, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
}

size_t EventDispatcher::dispatched_count() const {
    return dispatched_count_.load(std::memory_order_acquire);
}

size_t EventDispatcher::total_events() const {
    return events_.size();
}

const std::vector<MarkerDispatch>& EventDispatcher::marker_dispatches() const {
    return marker_dispatches_;
}

void EventDispatcher::run() {
    for (auto& event : events_) {
        if (cancelled_.load(std::memory_order_acquire)) {
            break;
        }

        auto target_time = epoch_ + std::chrono::duration<double>(event.time_sec);
        std::this_thread::sleep_until(target_time);

        if (!cancelled_.load(std::memory_order_acquire)) {
            event.callback();
            event.dispatched = true;
            dispatched_count_.fetch_add(1, std::memory_order_release);
        }
    }
    running_.store(false, std::memory_order_release);
}

} // namespace archerfish::runtime
