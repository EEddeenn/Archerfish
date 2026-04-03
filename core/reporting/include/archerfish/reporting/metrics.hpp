#pragma once

#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

namespace archerfish::reporting {

struct QueueStats {
    double avg_depth{0.0};
    double max_depth{0.0};
    size_t overflow_count{0};
};

struct Metrics {
    double start_requested_sec{0.0};
    double start_actual_sec{0.0};
    double stop_actual_sec{0.0};
    double tx_duration_sec{0.0};
    size_t underrun_count{0};
    size_t late_command_count{0};
    QueueStats queue_depth_stats;
    size_t warning_count{0};
    size_t error_count{0};
    size_t total_samples_sent{0};
    std::string scenario_hash;

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static Metrics from_json(const nlohmann::json& j);
};

} // namespace archerfish::reporting
