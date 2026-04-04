#include "archerfish/cli/cmd_calib.hpp"
#include "archerfish/reporting/calibration.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace archerfish::cli {

namespace {

std::string make_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_r(&time_t_now, &tm_buf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

std::string format_calibration_table(const reporting::CalibrationData& data) {
    std::ostringstream out;
    out << fmt::format("Device: {}  Channel: {}\n", data.device_id, data.channel);
    out << fmt::format("Timestamp: {}\n", data.timestamp);
    out << std::string(80, '-') << "\n";
    out << fmt::format("{:>12s} {:>10s} {:>14s} {:>14s} {:>10s}\n",
                       "Freq (Hz)", "Gain (dB)", "Measured (dBm)", "Expected (dBm)", "Error (dB)");
    out << std::string(80, '-') << "\n";
    for (const auto& e : data.entries) {
        out << fmt::format("{:12.1f} {:10.2f} {:14.2f} {:14.2f} {:10.2f}\n",
                           e.freq_hz, e.gain_db, e.measured_power_dbm,
                           e.expected_power_dbm, e.error_db);
    }
    if (data.entries.empty()) {
        out << "  (no calibration entries)\n";
    }
    return out.str();
}

} // namespace

int cmd_calib_init(const CliOptions& opts, const std::string& device_id, uint32_t channel) {
    (void)opts;
    try {
        reporting::CalibrationData data;
        data.device_id = device_id;
        data.channel = channel;
        data.timestamp = make_iso8601();

        auto dir = reporting::CalibrationData::calibration_dir();
        std::filesystem::create_directories(dir);

        auto path = reporting::CalibrationData::calibration_file(device_id, channel);
        if (std::filesystem::exists(path)) {
            fmt::print(stderr, "Calibration file already exists: {}\n", path.string());
            return static_cast<int>(ExitCode::InputValidationFailure);
        }

        std::ofstream f(path);
        f << data.to_json() << std::endl;
        fmt::print("Created calibration file: {}\n", path.string());
        return 0;
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }
}

int cmd_calib_show(const CliOptions& opts, const std::string& device_id, uint32_t channel, bool show_all_channels) {
    (void)opts;
    try {
        auto dir = reporting::CalibrationData::calibration_dir();

        if (show_all_channels) {
            if (!std::filesystem::exists(dir)) {
                fmt::print("No calibration directory found.\n");
                return 0;
            }
            bool found = false;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == ".json") {
                    std::ifstream f(entry.path());
                    std::string content((std::istreambuf_iterator<char>(f)),
                                        std::istreambuf_iterator<char>());
                    auto result = reporting::CalibrationData::from_json(content);
                    if (result.has_value()) {
                        if (opts.json_output) {
                            fmt::print("{}\n", result->to_json());
                        } else {
                            fmt::print("{}\n", format_calibration_table(*result));
                        }
                        found = true;
                    }
                }
            }
            if (!found) {
                fmt::print("No calibration files found.\n");
            }
        } else {
            auto path = reporting::CalibrationData::calibration_file(device_id, channel);
            if (!std::filesystem::exists(path)) {
                fmt::print(stderr, "Calibration file not found: {}\n", path.string());
                return static_cast<int>(ExitCode::InputValidationFailure);
            }
            std::ifstream f(path);
            std::string content((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
            auto result = reporting::CalibrationData::from_json(content);
            if (!result.has_value()) {
                fmt::print(stderr, "Invalid calibration file: {}\n", result.error());
                return static_cast<int>(ExitCode::InputValidationFailure);
            }
            if (opts.json_output) {
                fmt::print("{}\n", result->to_json());
            } else {
                fmt::print("{}\n", format_calibration_table(*result));
            }
        }
        return 0;
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }
}

int cmd_calib_import(const CliOptions& opts, const std::string& file_path, const std::string& device_id, uint32_t channel) {
    (void)opts;
    try {
        if (!std::filesystem::exists(file_path)) {
            fmt::print(stderr, "File not found: {}\n", file_path);
            return static_cast<int>(ExitCode::InputValidationFailure);
        }

        std::ifstream f(file_path);
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());

        auto result = reporting::CalibrationData::from_json(content);
        if (!result.has_value()) {
            fmt::print(stderr, "Invalid calibration file: {}\n", result.error());
            return static_cast<int>(ExitCode::InputValidationFailure);
        }

        auto dir = reporting::CalibrationData::calibration_dir();
        std::filesystem::create_directories(dir);

        reporting::CalibrationData imported = *result;
        imported.device_id = device_id;
        imported.channel = channel;

        auto dest = reporting::CalibrationData::calibration_file(device_id, channel);
        std::ofstream out(dest);
        out << imported.to_json() << std::endl;
        fmt::print("Imported calibration data to: {}\n", dest.string());
        return 0;
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }
}

} // namespace archerfish::cli
