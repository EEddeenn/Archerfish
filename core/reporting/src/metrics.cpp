#include "archerfish/reporting/metrics.hpp"

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace archerfish::reporting {

namespace {

double get_non_negative_finite_double(const nlohmann::json& j, const char* key) {
    const auto& raw = j.at(key);
    if (!raw.is_number()) {
        throw std::invalid_argument(std::string(key) + " must be a number");
    }
    double value = raw.get<double>();
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(std::string(key) + " must be finite and non-negative");
    }
    return value;
}

size_t get_count(const nlohmann::json& j, const char* key) {
    const auto& value = j.at(key);
    if (value.is_number_integer() && value.get<std::int64_t>() < 0) {
        throw std::invalid_argument(std::string(key) + " must be non-negative");
    }
    if (!value.is_number_unsigned() && !value.is_number_integer()) {
        throw std::invalid_argument(std::string(key) + " must be an integer count");
    }
    return value.get<size_t>();
}

std::string get_string(const nlohmann::json& j, const char* key) {
    const auto& value = j.at(key);
    if (!value.is_string()) {
        throw std::invalid_argument(std::string(key) + " must be a string");
    }
    return value.get<std::string>();
}

void require_object(const nlohmann::json& j, const char* name) {
    if (!j.is_object()) {
        throw std::invalid_argument(std::string(name) + " must be an object");
    }
}

void require_duration_matches_window(double start_sec, double stop_sec, double duration_sec) {
    if (stop_sec < start_sec) {
        throw std::invalid_argument("stop_actual_sec must be >= start_actual_sec");
    }

    const double expected_duration = stop_sec - start_sec;
    const double tolerance = 1e-9 * std::max({1.0, std::abs(stop_sec), std::abs(start_sec), std::abs(duration_sec)});
    if (std::abs(duration_sec - expected_duration) > tolerance) {
        throw std::invalid_argument("tx_duration_sec must match stop_actual_sec - start_actual_sec");
    }
}

} // namespace

nlohmann::json QueueStats_to_json(const QueueStats& qs) {
    return {
        {"avg_depth", qs.avg_depth},
        {"max_depth", qs.max_depth},
        {"overflow_count", qs.overflow_count},
    };
}

QueueStats QueueStats_from_json(const nlohmann::json& j) {
    require_object(j, "queue_depth_stats");

    QueueStats qs;
    qs.avg_depth = get_non_negative_finite_double(j, "avg_depth");
    qs.max_depth = get_non_negative_finite_double(j, "max_depth");
    qs.overflow_count = get_count(j, "overflow_count");
    if (qs.avg_depth > qs.max_depth) {
        throw std::invalid_argument("avg_depth must be <= max_depth");
    }
    return qs;
}

nlohmann::json Metrics::to_json() const {
    return {
        {"start_requested_sec", start_requested_sec},
        {"start_actual_sec", start_actual_sec},
        {"stop_actual_sec", stop_actual_sec},
        {"tx_duration_sec", tx_duration_sec},
        {"underrun_count", underrun_count},
        {"late_command_count", late_command_count},
        {"queue_depth_stats", QueueStats_to_json(queue_depth_stats)},
        {"warning_count", warning_count},
        {"error_count", error_count},
        {"total_samples_sent", total_samples_sent},
        {"scenario_hash", scenario_hash},
    };
}

Metrics Metrics::from_json(const nlohmann::json& j) {
    require_object(j, "metrics");

    Metrics m;
    m.start_requested_sec = get_non_negative_finite_double(j, "start_requested_sec");
    m.start_actual_sec = get_non_negative_finite_double(j, "start_actual_sec");
    m.stop_actual_sec = get_non_negative_finite_double(j, "stop_actual_sec");
    m.tx_duration_sec = get_non_negative_finite_double(j, "tx_duration_sec");
    m.underrun_count = get_count(j, "underrun_count");
    m.late_command_count = get_count(j, "late_command_count");
    m.queue_depth_stats = QueueStats_from_json(j.at("queue_depth_stats"));
    m.warning_count = get_count(j, "warning_count");
    m.error_count = get_count(j, "error_count");
    m.total_samples_sent = get_count(j, "total_samples_sent");
    m.scenario_hash = get_string(j, "scenario_hash");
    require_duration_matches_window(m.start_actual_sec, m.stop_actual_sec, m.tx_duration_sec);
    return m;
}

} // namespace archerfish::reporting
