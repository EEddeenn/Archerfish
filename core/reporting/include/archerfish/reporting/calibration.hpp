#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace archerfish::reporting {

struct CalibrationPoint {
    double freq_hz{0.0};
    double gain_db{0.0};
    double measured_power_dbm{0.0};
    double expected_power_dbm{0.0};
    double error_db{0.0};
};

struct InterpolationResult {
    double estimated_power_dbm{0.0};
    std::string method{"none"};
};

struct CalibrationData {
    std::string device_id;
    uint32_t channel{0};
    std::string timestamp;
    std::vector<CalibrationPoint> entries;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static std::expected<CalibrationData, std::string> from_json(const std::string& json);
    [[nodiscard]] static std::filesystem::path calibration_dir();
    [[nodiscard]] static std::filesystem::path calibration_file(const std::string& device_id, uint32_t channel);

    [[nodiscard]] std::optional<InterpolationResult> interpolate_power(double freq_hz, double gain_db) const;

    [[nodiscard]] double compute_required_amplitude(double target_power_dbm, double freq_hz, double gain_db) const;
};

} // namespace archerfish::reporting
