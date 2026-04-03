#include "archerfish/reporting/metrics.hpp"

namespace archerfish::reporting {

nlohmann::json QueueStats_to_json(const QueueStats& qs) {
    return {
        {"avg_depth", qs.avg_depth},
        {"max_depth", qs.max_depth},
        {"overflow_count", qs.overflow_count},
    };
}

QueueStats QueueStats_from_json(const nlohmann::json& j) {
    QueueStats qs;
    qs.avg_depth = j.at("avg_depth").get<double>();
    qs.max_depth = j.at("max_depth").get<double>();
    qs.overflow_count = j.at("overflow_count").get<size_t>();
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
    Metrics m;
    m.start_requested_sec = j.at("start_requested_sec").get<double>();
    m.start_actual_sec = j.at("start_actual_sec").get<double>();
    m.stop_actual_sec = j.at("stop_actual_sec").get<double>();
    m.tx_duration_sec = j.at("tx_duration_sec").get<double>();
    m.underrun_count = j.at("underrun_count").get<size_t>();
    m.late_command_count = j.at("late_command_count").get<size_t>();
    m.queue_depth_stats = QueueStats_from_json(j.at("queue_depth_stats"));
    m.warning_count = j.at("warning_count").get<size_t>();
    m.error_count = j.at("error_count").get<size_t>();
    m.total_samples_sent = j.at("total_samples_sent").get<size_t>();
    m.scenario_hash = j.at("scenario_hash").get<std::string>();
    return m;
}

} // namespace archerfish::reporting
