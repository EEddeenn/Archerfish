#include "archerfish/reporting/emitter_metrics.hpp"

namespace archerfish::reporting {

nlohmann::json to_json(const EmitterMetrics& m) {
    return {
        {"emitter_id", m.emitter_id},
        {"device_id", m.device_id},
        {"start_time_sec", m.start_time_sec},
        {"duration_sec", m.duration_sec},
        {"samples_rendered", m.samples_rendered},
        {"peak_amplitude", m.peak_amplitude},
        {"rms_amplitude", m.rms_amplitude},
        {"crest_factor", m.crest_factor},
        {"nominal_bandwidth", m.nominal_bandwidth},
        {"waveform_type", m.waveform_type},
        {"completed", m.completed},
    };
}

nlohmann::json to_json(const ScenarioMetricsSummary& s) {
    nlohmann::json emitters_obj = nlohmann::json::object();
    for (const auto& [id, m] : s.emitters) {
        emitters_obj[id] = to_json(m);
    }

    return {
        {"total_emitters", s.total_emitters},
        {"completed_emitters", s.completed_emitters},
        {"total_duration_sec", s.total_duration_sec},
        {"scenario_start_sec", s.scenario_start_sec},
        {"scenario_end_sec", s.scenario_end_sec},
        {"emitters", emitters_obj},
    };
}

ScenarioMetricsSummary from_json(const nlohmann::json& j) {
    ScenarioMetricsSummary s;
    s.total_emitters = j.at("total_emitters").get<int>();
    s.completed_emitters = j.at("completed_emitters").get<int>();
    s.total_duration_sec = j.at("total_duration_sec").get<double>();
    s.scenario_start_sec = j.at("scenario_start_sec").get<double>();
    s.scenario_end_sec = j.at("scenario_end_sec").get<double>();

    const auto& emitters_obj = j.at("emitters");
    for (auto it = emitters_obj.begin(); it != emitters_obj.end(); ++it) {
        const auto& ej = it.value();
        EmitterMetrics m;
        m.emitter_id = ej.at("emitter_id").get<std::string>();
        m.device_id = ej.at("device_id").get<std::string>();
        m.start_time_sec = ej.at("start_time_sec").get<double>();
        m.duration_sec = ej.at("duration_sec").get<double>();
        m.samples_rendered = ej.at("samples_rendered").get<size_t>();
        m.peak_amplitude = ej.at("peak_amplitude").get<double>();
        m.rms_amplitude = ej.at("rms_amplitude").get<double>();
        m.crest_factor = ej.at("crest_factor").get<double>();
        m.nominal_bandwidth = ej.at("nominal_bandwidth").get<double>();
        m.waveform_type = ej.at("waveform_type").get<std::string>();
        m.completed = ej.at("completed").get<bool>();
        s.emitters[it.key()] = std::move(m);
    }

    return s;
}

} // namespace archerfish::reporting
