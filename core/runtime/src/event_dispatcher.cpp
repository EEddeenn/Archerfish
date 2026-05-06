#include "archerfish/runtime/event_dispatcher.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace archerfish::runtime {

EventDispatcher::~EventDispatcher() {
    cancel();
}

void EventDispatcher::schedule(double time_sec, std::function<void()> callback) {
    if (!std::isfinite(time_sec) || time_sec < 0.0) {
        throw std::invalid_argument("Event time must be finite and non-negative");
    }
    if (!callback) {
        throw std::invalid_argument("Event callback must not be empty");
    }
    events_.push_back({time_sec, std::move(callback), false});
}

void EventDispatcher::start() {
    if (thread_.joinable()) {
        throw std::logic_error("EventDispatcher is already running");
    }
    std::sort(events_.begin(), events_.end(),
              [](const ScheduledEvent& a, const ScheduledEvent& b) {
                  return a.time_sec < b.time_sec;
              });

    cancelled_.store(false, std::memory_order_release);
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
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

size_t EventDispatcher::dispatched_count() const {
    return dispatched_count_.load(std::memory_order_acquire);
}

size_t EventDispatcher::failed_count() const {
    return failed_count_.load(std::memory_order_acquire);
}

bool EventDispatcher::has_failed() const {
    return failed_count() > 0;
}

size_t EventDispatcher::total_events() const {
    return events_.size();
}

const std::vector<MarkerDispatch>& EventDispatcher::marker_dispatches() const {
    return marker_dispatches_;
}

const std::vector<WaveformSwitchDispatch>& EventDispatcher::waveform_switch_dispatches() const {
    return waveform_switch_dispatches_;
}

const std::vector<ImpairmentChangeDispatch>& EventDispatcher::impairment_change_dispatches() const {
    return impairment_change_dispatches_;
}

void EventDispatcher::run() {
    for (auto& event : events_) {
        if (cancelled_.load(std::memory_order_acquire)) {
            break;
        }

        const auto target_time = epoch_ + std::chrono::duration<double>(event.time_sec);
        std::unique_lock lock(mutex_);
        cv_.wait_until(lock, target_time, [this] {
            return cancelled_.load(std::memory_order_acquire);
        });
        lock.unlock();

        if (!cancelled_.load(std::memory_order_acquire)) {
            try {
                event.callback();
                event.dispatched = true;
                dispatched_count_.fetch_add(1, std::memory_order_release);
            } catch (const std::exception& e) {
                failed_count_.fetch_add(1, std::memory_order_release);
                spdlog::error("Scheduled event at {:.6f}s failed: {}", event.time_sec, e.what());
            } catch (...) {
                failed_count_.fetch_add(1, std::memory_order_release);
                spdlog::error("Scheduled event at {:.6f}s failed with an unknown error", event.time_sec);
            }
        }
    }
    running_.store(false, std::memory_order_release);
}

} // namespace archerfish::runtime
