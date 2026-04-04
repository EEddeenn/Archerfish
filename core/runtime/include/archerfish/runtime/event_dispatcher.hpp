#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>

namespace archerfish::runtime {

struct MarkerDispatch {
    std::string name;
    double planned_time_sec{0.0};
    double wall_clock_sec{0.0};
};

struct ScheduledEvent {
    double time_sec{0.0};
    std::function<void()> callback;
    bool dispatched{false};
};

class EventDispatcher {
public:
    EventDispatcher() = default;

    void schedule(double time_sec, std::function<void()> callback);
    void start();
    void wait_complete();
    void cancel();

    [[nodiscard]] size_t dispatched_count() const;
    [[nodiscard]] size_t total_events() const;
    [[nodiscard]] const std::vector<MarkerDispatch>& marker_dispatches() const;

private:
    void run();
    std::vector<ScheduledEvent> events_;
    std::vector<MarkerDispatch> marker_dispatches_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<size_t> dispatched_count_{0};
    std::chrono::steady_clock::time_point epoch_;
};

} // namespace archerfish::runtime
