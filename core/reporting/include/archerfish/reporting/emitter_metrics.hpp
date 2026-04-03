#pragma once

#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace archerfish::reporting {

struct EmitterMetrics {
    std::string emitter_id;
    std::string device_id;
    double start_time_sec{0};
    double duration_sec{0};
    size_t samples_rendered{0};
    double peak_amplitude{0};
    double rms_amplitude{0};
    double crest_factor{0};
    double nominal_bandwidth{0};
    std::string waveform_type;
    bool completed{false};
};

/// Aggregate per-emitter metrics into summary
struct ScenarioMetricsSummary {
    int total_emitters{0};
    int completed_emitters{0};
    double total_duration_sec{0};
    double scenario_start_sec{0};
    double scenario_end_sec{0};
    std::map<std::string, EmitterMetrics> emitters;
};

nlohmann::json to_json(const EmitterMetrics& m);
nlohmann::json to_json(const ScenarioMetricsSummary& s);
ScenarioMetricsSummary from_json(const nlohmann::json& j);

} // namespace archerfish::reporting
