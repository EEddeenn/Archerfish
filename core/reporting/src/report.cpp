#include "archerfish/reporting/report.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fmt/format.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "archerfish/scenario/plan_io.hpp"

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

uint32_t get_uint32(const nlohmann::json& j, const char* key) {
    const auto& value = j.at(key);
    if (value.is_number_integer() && value.get<std::int64_t>() < 0) {
        throw std::invalid_argument(std::string(key) + " must be non-negative");
    }
    if (!value.is_number_unsigned() && !value.is_number_integer()) {
        throw std::invalid_argument(std::string(key) + " must be an integer");
    }
    const auto raw = value.get<uint64_t>();
    if (raw > std::numeric_limits<uint32_t>::max()) {
        throw std::invalid_argument(std::string(key) + " is out of uint32 range");
    }
    return static_cast<uint32_t>(raw);
}

std::string get_string(const nlohmann::json& j, const char* key) {
    const auto& value = j.at(key);
    if (!value.is_string()) {
        throw std::invalid_argument(std::string(key) + " must be a string");
    }
    return value.get<std::string>();
}

std::vector<std::string> get_string_array(const nlohmann::json& j, const char* key) {
    const auto& value = j.at(key);
    if (!value.is_array()) {
        throw std::invalid_argument(std::string(key) + " must be an array");
    }
    std::vector<std::string> result;
    result.reserve(value.size());
    for (const auto& item : value) {
        if (!item.is_string()) {
            throw std::invalid_argument(std::string(key) + " entries must be strings");
        }
        result.push_back(item.get<std::string>());
    }
    return result;
}

const nlohmann::json& get_array(const nlohmann::json& j, const char* key) {
    const auto& value = j.at(key);
    if (!value.is_array()) {
        throw std::invalid_argument(std::string(key) + " must be an array");
    }
    return value;
}

void require_object(const nlohmann::json& j, const char* name) {
    if (!j.is_object()) {
        throw std::invalid_argument(std::string(name) + " must be an object");
    }
}

void require_duration_matches_window(double start_sec, double stop_sec, double duration_sec) {
    if (stop_sec < start_sec) {
        throw std::invalid_argument("actual_stop_sec must be >= actual_start_sec");
    }

    const double expected_duration = stop_sec - start_sec;
    const double tolerance = 1e-9 * std::max({1.0, std::abs(stop_sec), std::abs(start_sec), std::abs(duration_sec)});
    if (std::abs(duration_sec - expected_duration) > tolerance) {
        throw std::invalid_argument("actual_duration_sec must match actual_stop_sec - actual_start_sec");
    }
}

} // namespace

nlohmann::json DeviceInfo_to_json(const DeviceInfo& di) {
    return {
        {"device_id", di.device_id},
        {"device_type", di.device_type},
        {"channel", di.channel},
    };
}

DeviceInfo DeviceInfo_from_json(const nlohmann::json& j) {
    require_object(j, "device");

    DeviceInfo di;
    di.device_id = get_string(j, "device_id");
    di.device_type = get_string(j, "device_type");
    di.channel = get_uint32(j, "channel");
    return di;
}

nlohmann::json Report::to_json() const {
    nlohmann::json devices_arr = nlohmann::json::array();
    for (const auto& d : devices) {
        devices_arr.push_back(DeviceInfo_to_json(d));
    }

    nlohmann::json warnings_arr = nlohmann::json::array();
    for (const auto& w : warnings) {
        warnings_arr.push_back(scenario::error_to_json(w));
    }

    nlohmann::json errors_arr = nlohmann::json::array();
    for (const auto& e : errors) {
        errors_arr.push_back(scenario::error_to_json(e));
    }

    nlohmann::json markers_arr = nlohmann::json::array();
    for (const auto& m : marker_events) {
        markers_arr.push_back({
            {"name", m.name},
            {"planned_time_sec", m.planned_time_sec},
            {"wall_clock_sec", m.wall_clock_sec},
        });
    }

    return {
        {"scenario_name", scenario_name},
        {"scenario_hash", scenario_hash},
        {"status", status},
        {"planned_start_sec", planned_start_sec},
        {"actual_start_sec", actual_start_sec},
        {"actual_stop_sec", actual_stop_sec},
        {"actual_duration_sec", actual_duration_sec},
        {"devices", devices_arr},
        {"warnings", warnings_arr},
        {"errors", errors_arr},
        {"artifact_paths", artifact_paths},
        {"marker_events", markers_arr},
    };
}

Report Report::from_json(const nlohmann::json& j) {
    require_object(j, "report");

    Report r;
    r.scenario_name = get_string(j, "scenario_name");
    r.scenario_hash = get_string(j, "scenario_hash");
    r.status = get_string(j, "status");
    r.planned_start_sec = get_non_negative_finite_double(j, "planned_start_sec");
    r.actual_start_sec = get_non_negative_finite_double(j, "actual_start_sec");
    r.actual_stop_sec = get_non_negative_finite_double(j, "actual_stop_sec");
    r.actual_duration_sec = get_non_negative_finite_double(j, "actual_duration_sec");
    require_duration_matches_window(r.actual_start_sec, r.actual_stop_sec, r.actual_duration_sec);

    for (const auto& d : get_array(j, "devices")) {
        r.devices.push_back(DeviceInfo_from_json(d));
    }

    for (const auto& w : get_array(j, "warnings")) {
        require_object(w, "warning");
        r.warnings.push_back(scenario::error_from_json(w));
    }

    for (const auto& e : get_array(j, "errors")) {
        require_object(e, "error");
        r.errors.push_back(scenario::error_from_json(e));
    }

    r.artifact_paths = get_string_array(j, "artifact_paths");

    if (j.contains("marker_events")) {
        for (const auto& mj : get_array(j, "marker_events")) {
            require_object(mj, "marker_event");

            MarkerRecord mr;
            mr.name = get_string(mj, "name");
            mr.planned_time_sec = get_non_negative_finite_double(mj, "planned_time_sec");
            mr.wall_clock_sec = get_non_negative_finite_double(mj, "wall_clock_sec");
            r.marker_events.push_back(std::move(mr));
        }
    }

    return r;
}

std::string Report::summary() const {
    std::string result;
    result += fmt::format("Scenario: {}\n", scenario_name);
    result += fmt::format("Status:   {}\n", status);
    result += fmt::format("Duration: {:.3f}s\n", actual_duration_sec);
    result += fmt::format("Devices:  {}\n", devices.size());
    result += fmt::format("Warnings: {}\n", warnings.size());
    result += fmt::format("Errors:   {}\n", errors.size());
    result += fmt::format("Artifacts: {}\n", artifact_paths.size());
    return result;
}

} // namespace archerfish::reporting
