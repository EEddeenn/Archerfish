#include "archerfish/reporting/report.hpp"

#include <fmt/format.h>

#include "archerfish/scenario/plan_io.hpp"

namespace archerfish::reporting {

nlohmann::json DeviceInfo_to_json(const DeviceInfo& di) {
    return {
        {"device_id", di.device_id},
        {"device_type", di.device_type},
        {"channel", di.channel},
    };
}

DeviceInfo DeviceInfo_from_json(const nlohmann::json& j) {
    DeviceInfo di;
    di.device_id = j.at("device_id").get<std::string>();
    di.device_type = j.at("device_type").get<std::string>();
    di.channel = j.at("channel").get<uint32_t>();
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
    Report r;
    r.scenario_name = j.at("scenario_name").get<std::string>();
    r.scenario_hash = j.at("scenario_hash").get<std::string>();
    r.status = j.at("status").get<std::string>();
    r.planned_start_sec = j.at("planned_start_sec").get<double>();
    r.actual_start_sec = j.at("actual_start_sec").get<double>();
    r.actual_stop_sec = j.at("actual_stop_sec").get<double>();
    r.actual_duration_sec = j.at("actual_duration_sec").get<double>();

    for (const auto& d : j.at("devices")) {
        r.devices.push_back(DeviceInfo_from_json(d));
    }

    for (const auto& w : j.at("warnings")) {
        r.warnings.push_back(scenario::error_from_json(w));
    }

    for (const auto& e : j.at("errors")) {
        r.errors.push_back(scenario::error_from_json(e));
    }

    r.artifact_paths = j.at("artifact_paths").get<std::vector<std::string>>();

    if (j.contains("marker_events") && j.at("marker_events").is_array()) {
        for (const auto& mj : j.at("marker_events")) {
            MarkerRecord mr;
            mr.name = mj.at("name").get<std::string>();
            mr.planned_time_sec = mj.at("planned_time_sec").get<double>();
            mr.wall_clock_sec = mj.at("wall_clock_sec").get<double>();
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
