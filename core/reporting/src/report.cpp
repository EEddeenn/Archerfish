#include "archerfish/reporting/report.hpp"

#include <fmt/format.h>

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

nlohmann::json Error_to_json(const common::Error& e) {
    return {
        {"category", common::category_to_string(e.category)},
        {"code", e.code},
        {"message", e.message},
    };
}

common::Error Error_from_json(const nlohmann::json& j) {
    common::Error e;
    e.category = common::category_from_string(j.value("category", "Config"));
    e.code = j.at("code").get<std::string>();
    e.message = j.at("message").get<std::string>();
    return e;
}

nlohmann::json Report::to_json() const {
    nlohmann::json devices_arr = nlohmann::json::array();
    for (const auto& d : devices) {
        devices_arr.push_back(DeviceInfo_to_json(d));
    }

    nlohmann::json warnings_arr = nlohmann::json::array();
    for (const auto& w : warnings) {
        warnings_arr.push_back(Error_to_json(w));
    }

    nlohmann::json errors_arr = nlohmann::json::array();
    for (const auto& e : errors) {
        errors_arr.push_back(Error_to_json(e));
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
        r.warnings.push_back(Error_from_json(w));
    }

    for (const auto& e : j.at("errors")) {
        r.errors.push_back(Error_from_json(e));
    }

    r.artifact_paths = j.at("artifact_paths").get<std::vector<std::string>>();
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
