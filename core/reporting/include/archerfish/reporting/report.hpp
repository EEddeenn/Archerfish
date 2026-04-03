#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/common/error.hpp"

namespace archerfish::reporting {

struct DeviceInfo {
    std::string device_id;
    std::string device_type;
    uint32_t channel{0};
};

struct Report {
    std::string scenario_name;
    std::string scenario_hash;
    std::string status;
    double planned_start_sec{0.0};
    double actual_start_sec{0.0};
    double actual_stop_sec{0.0};
    double actual_duration_sec{0.0};
    std::vector<DeviceInfo> devices;
    common::ErrorList warnings;
    common::ErrorList errors;
    std::vector<std::string> artifact_paths;

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static Report from_json(const nlohmann::json& j);

    [[nodiscard]] std::string summary() const;
};

} // namespace archerfish::reporting
