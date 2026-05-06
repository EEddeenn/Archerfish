#include "archerfish/reporting/emitter_metrics.hpp"

#include <cmath>
#include <cctype>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

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

int get_non_negative_int(const nlohmann::json& j, const char* key) {
    const auto& raw = j.at(key);
    if (!raw.is_number_integer() && !raw.is_number_unsigned()) {
        throw std::invalid_argument(std::string(key) + " must be an integer");
    }
    if (raw.is_number_integer()) {
        const auto parsed = raw.get<std::int64_t>();
        if (parsed < 0) {
            throw std::invalid_argument(std::string(key) + " must be non-negative");
        }
        if (parsed > std::numeric_limits<int>::max()) {
            throw std::invalid_argument(std::string(key) + " is out of int range");
        }
        return static_cast<int>(parsed);
    }
    const auto parsed = raw.get<std::uint64_t>();
    if (parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument(std::string(key) + " is out of int range");
    }
    int value = static_cast<int>(parsed);
    if (value < 0) {
        throw std::invalid_argument(std::string(key) + " must be non-negative");
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

bool is_blank(std::string_view value) {
    for (unsigned char ch : value) {
        if (std::isspace(ch) == 0) {
            return false;
        }
    }
    return true;
}

bool get_bool(const nlohmann::json& j, const char* key) {
    const auto& value = j.at(key);
    if (!value.is_boolean()) {
        throw std::invalid_argument(std::string(key) + " must be a boolean");
    }
    return value.get<bool>();
}

void require_object(const nlohmann::json& j, const char* name) {
    if (!j.is_object()) {
        throw std::invalid_argument(std::string(name) + " must be an object");
    }
}

} // namespace

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
    require_object(j, "scenario_metrics_summary");

    ScenarioMetricsSummary s;
    s.total_emitters = get_non_negative_int(j, "total_emitters");
    s.completed_emitters = get_non_negative_int(j, "completed_emitters");
    s.total_duration_sec = get_non_negative_finite_double(j, "total_duration_sec");
    s.scenario_start_sec = get_non_negative_finite_double(j, "scenario_start_sec");
    s.scenario_end_sec = get_non_negative_finite_double(j, "scenario_end_sec");
    if (s.scenario_end_sec < s.scenario_start_sec) {
        throw std::invalid_argument("scenario_end_sec must be >= scenario_start_sec");
    }

    const auto& emitters_obj = j.at("emitters");
    if (!emitters_obj.is_object()) {
        throw std::invalid_argument("emitters must be an object");
    }
    int completed_count = 0;
    for (auto it = emitters_obj.begin(); it != emitters_obj.end(); ++it) {
        if (it.key().empty() || is_blank(it.key())) {
            throw std::invalid_argument("emitter object keys must be non-blank");
        }
        const auto& ej = it.value();
        if (!ej.is_object()) {
            throw std::invalid_argument("emitter entries must be objects");
        }
        EmitterMetrics m;
        m.emitter_id = get_string(ej, "emitter_id");
        if (m.emitter_id.empty() || is_blank(m.emitter_id)) {
            throw std::invalid_argument("emitter_id must be non-blank");
        }
        m.device_id = get_string(ej, "device_id");
        m.start_time_sec = get_non_negative_finite_double(ej, "start_time_sec");
        m.duration_sec = get_non_negative_finite_double(ej, "duration_sec");
        m.samples_rendered = get_count(ej, "samples_rendered");
        m.peak_amplitude = get_non_negative_finite_double(ej, "peak_amplitude");
        m.rms_amplitude = get_non_negative_finite_double(ej, "rms_amplitude");
        m.crest_factor = get_non_negative_finite_double(ej, "crest_factor");
        m.nominal_bandwidth = get_non_negative_finite_double(ej, "nominal_bandwidth");
        m.waveform_type = get_string(ej, "waveform_type");
        m.completed = get_bool(ej, "completed");
        if (m.emitter_id != it.key()) {
            throw std::invalid_argument("emitter_id must match emitters object key");
        }
        if (m.completed) {
            ++completed_count;
        }
        s.emitters[it.key()] = std::move(m);
    }
    if (s.total_emitters != static_cast<int>(s.emitters.size())) {
        throw std::invalid_argument("total_emitters must match emitters size");
    }
    if (s.completed_emitters != completed_count) {
        throw std::invalid_argument("completed_emitters must match completed emitter count");
    }

    return s;
}

} // namespace archerfish::reporting
