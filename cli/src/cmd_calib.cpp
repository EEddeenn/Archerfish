#include "archerfish/cli/cmd_calib.hpp"
#include "archerfish/reporting/calibration.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <expected>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

std::expected<void, std::string> validate_calibration_device_id(const std::string& device_id) {
    auto is_blank = [](std::string_view value) {
        return std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    };

    if (device_id.empty() || is_blank(device_id)) {
        return std::unexpected("Device ID must not be empty");
    }
    for (unsigned char ch : device_id) {
        if (ch == '/' || ch == '\\' || std::iscntrl(ch) != 0) {
            return std::unexpected("Device ID must not contain path separators or control characters");
        }
    }
    return {};
}

std::expected<std::string, std::string> read_text_file(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return std::unexpected(fmt::format("Cannot open file: {}", path.string()));
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    if (f.bad()) {
        return std::unexpected(fmt::format("Failed while reading file: {}", path.string()));
    }
    return ss.str();
}

std::expected<void, std::string> write_text_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) {
        return std::unexpected(fmt::format("Cannot open file for writing: {}", path.string()));
    }
    f << content << '\n';
    if (!f) {
        return std::unexpected(fmt::format("Failed while writing file: {}", path.string()));
    }
    return {};
}

std::expected<bool, std::string> path_exists(const std::filesystem::path& path) {
    std::error_code fs_error;
    const bool exists = std::filesystem::exists(path, fs_error);
    if (fs_error) {
        return std::unexpected(fmt::format("Cannot inspect path '{}': {}", path.string(), fs_error.message()));
    }
    return exists;
}

std::expected<void, std::string> ensure_directory(const std::filesystem::path& dir) {
    std::error_code fs_error;
    std::filesystem::create_directories(dir, fs_error);
    if (fs_error) {
        return std::unexpected(fmt::format("Cannot create directory '{}': {}", dir.string(), fs_error.message()));
    }
    return {};
}

} // namespace

int cmd_calib_init(const CliOptions& opts, const std::string& device_id, uint32_t channel) {
    (void)opts;
    try {
        auto device_id_valid = validate_calibration_device_id(device_id);
        if (!device_id_valid.has_value()) {
            fmt::print(stderr, "Invalid device ID: {}\n", device_id_valid.error());
            return static_cast<int>(ExitCode::InputValidationFailure);
        }

        reporting::CalibrationData data;
        data.device_id = device_id;
        data.channel = channel;
        data.timestamp = make_iso8601();

        auto dir = reporting::CalibrationData::calibration_dir();
        auto dir_result = ensure_directory(dir);
        if (!dir_result.has_value()) {
            fmt::print(stderr, "Error: {}\n", dir_result.error());
            return static_cast<int>(ExitCode::GenericFailure);
        }

        auto path = reporting::CalibrationData::calibration_file(device_id, channel);
        auto exists = path_exists(path);
        if (!exists.has_value()) {
            fmt::print(stderr, "Error: {}\n", exists.error());
            return static_cast<int>(ExitCode::GenericFailure);
        }
        if (*exists) {
            fmt::print(stderr, "Calibration file already exists: {}\n", path.string());
            return static_cast<int>(ExitCode::InputValidationFailure);
        }

        auto write_result = write_text_file(path, data.to_json());
        if (!write_result.has_value()) {
            fmt::print(stderr, "Error: {}\n", write_result.error());
            return static_cast<int>(ExitCode::GenericFailure);
        }
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
            if (!device_id.empty()) {
                auto device_id_valid = validate_calibration_device_id(device_id);
                if (!device_id_valid.has_value()) {
                    fmt::print(stderr, "Invalid device ID: {}\n", device_id_valid.error());
                    return static_cast<int>(ExitCode::InputValidationFailure);
                }
            }

            auto dir_exists = path_exists(dir);
            if (!dir_exists.has_value()) {
                fmt::print(stderr, "Error: {}\n", dir_exists.error());
                return static_cast<int>(ExitCode::GenericFailure);
            }
            if (!*dir_exists) {
                fmt::print("No calibration directory found.\n");
                return 0;
            }
            std::error_code iter_error;
            std::filesystem::directory_iterator it(dir, iter_error);
            if (iter_error) {
                fmt::print(stderr, "Error: Cannot read calibration directory '{}': {}\n",
                           dir.string(), iter_error.message());
                return static_cast<int>(ExitCode::GenericFailure);
            }
            std::vector<std::filesystem::path> files;
            for (const auto end = std::filesystem::directory_iterator(); it != end; it.increment(iter_error)) {
                if (iter_error) {
                    fmt::print(stderr, "Error: Cannot read calibration directory '{}': {}\n",
                               dir.string(), iter_error.message());
                    return static_cast<int>(ExitCode::GenericFailure);
                }
                const auto& entry = *it;
                if (entry.path().extension() == ".json") {
                    files.push_back(entry.path());
                }
            }
            std::sort(files.begin(), files.end());

            bool found = false;
            nlohmann::json json_results = nlohmann::json::array();
            for (const auto& path : files) {
                auto content = read_text_file(path);
                if (!content.has_value()) {
                    fmt::print(stderr, "Skipping calibration file: {}\n", content.error());
                    continue;
                }
                auto result = reporting::CalibrationData::from_json(*content);
                if (result.has_value()) {
                    if (!device_id.empty() && result->device_id != device_id) {
                        continue;
                    }
                    if (opts.json_output) {
                        json_results.push_back(nlohmann::json::parse(result->to_json()));
                    } else {
                        fmt::print("{}\n", format_calibration_table(*result));
                    }
                    found = true;
                }
            }
            if (opts.json_output) {
                fmt::print("{}\n", json_results.dump(2));
            } else if (!found) {
                fmt::print("No calibration files found.\n");
            }
        } else {
            auto device_id_valid = validate_calibration_device_id(device_id);
            if (!device_id_valid.has_value()) {
                fmt::print(stderr, "Invalid device ID: {}\n", device_id_valid.error());
                return static_cast<int>(ExitCode::InputValidationFailure);
            }

            auto path = reporting::CalibrationData::calibration_file(device_id, channel);
            auto exists = path_exists(path);
            if (!exists.has_value()) {
                fmt::print(stderr, "Error: {}\n", exists.error());
                return static_cast<int>(ExitCode::GenericFailure);
            }
            if (!*exists) {
                fmt::print(stderr, "Calibration file not found: {}\n", path.string());
                return static_cast<int>(ExitCode::InputValidationFailure);
            }
            auto content = read_text_file(path);
            if (!content.has_value()) {
                fmt::print(stderr, "Error: {}\n", content.error());
                return static_cast<int>(ExitCode::InputValidationFailure);
            }
            auto result = reporting::CalibrationData::from_json(*content);
            if (!result.has_value()) {
                fmt::print(stderr, "Invalid calibration file: {}\n", result.error());
                return static_cast<int>(ExitCode::InputValidationFailure);
            }
            if (result->device_id != device_id || result->channel != channel) {
                fmt::print(stderr,
                           "Invalid calibration file: metadata device/channel '{}:{}' does not match requested '{}:{}'\n",
                           result->device_id, result->channel, device_id, channel);
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
        auto device_id_valid = validate_calibration_device_id(device_id);
        if (!device_id_valid.has_value()) {
            fmt::print(stderr, "Invalid device ID: {}\n", device_id_valid.error());
            return static_cast<int>(ExitCode::InputValidationFailure);
        }

        auto source_exists = path_exists(file_path);
        if (!source_exists.has_value()) {
            fmt::print(stderr, "Error: {}\n", source_exists.error());
            return static_cast<int>(ExitCode::GenericFailure);
        }
        if (!*source_exists) {
            fmt::print(stderr, "File not found: {}\n", file_path);
            return static_cast<int>(ExitCode::InputValidationFailure);
        }

        auto content = read_text_file(file_path);
        if (!content.has_value()) {
            fmt::print(stderr, "Error: {}\n", content.error());
            return static_cast<int>(ExitCode::InputValidationFailure);
        }

        auto result = reporting::CalibrationData::from_json(*content);
        if (!result.has_value()) {
            fmt::print(stderr, "Invalid calibration file: {}\n", result.error());
            return static_cast<int>(ExitCode::InputValidationFailure);
        }

        auto dir = reporting::CalibrationData::calibration_dir();
        auto dir_result = ensure_directory(dir);
        if (!dir_result.has_value()) {
            fmt::print(stderr, "Error: {}\n", dir_result.error());
            return static_cast<int>(ExitCode::GenericFailure);
        }

        reporting::CalibrationData imported = *result;
        imported.device_id = device_id;
        imported.channel = channel;

        auto dest = reporting::CalibrationData::calibration_file(device_id, channel);
        auto write_result = write_text_file(dest, imported.to_json());
        if (!write_result.has_value()) {
            fmt::print(stderr, "Error: {}\n", write_result.error());
            return static_cast<int>(ExitCode::GenericFailure);
        }
        fmt::print("Imported calibration data to: {}\n", dest.string());
        return 0;
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }
}

} // namespace archerfish::cli
