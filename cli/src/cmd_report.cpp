#include "archerfish/cli/cmd_report.hpp"
#include "archerfish/cli/format.hpp"
#include "archerfish/reporting/report.hpp"
#include "archerfish/reporting/emitter_metrics.hpp"
#include "archerfish/reporting/metrics.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace archerfish::cli {

namespace {

void flatten_json(const nlohmann::json& j,
                  const std::string& prefix,
                  std::vector<std::pair<std::string, std::string>>& rows) {
    for (auto& [key, value] : j.items()) {
        std::string full_key = prefix.empty() ? key : prefix + "." + key;
        if (value.is_object()) {
            flatten_json(value, full_key, rows);
        } else if (value.is_array()) {
            for (size_t i = 0; i < value.size(); ++i) {
                std::string idx_key = full_key + "." + std::to_string(i);
                if (value[i].is_object()) {
                    flatten_json(value[i], idx_key, rows);
                } else if (value[i].is_string()) {
                    rows.emplace_back(idx_key, value[i].get<std::string>());
                } else {
                    rows.emplace_back(idx_key, value[i].dump());
                }
            }
        } else if (value.is_string()) {
            rows.emplace_back(full_key, value.get<std::string>());
        } else {
            rows.emplace_back(full_key, value.dump());
        }
    }
}

static std::string csv_escape(const std::string& s) {
    if (s.find(',') != std::string::npos || s.find('"') != std::string::npos || s.find('\n') != std::string::npos) {
        std::string escaped;
        escaped += '"';
        for (char c : s) {
            if (c == '"') escaped += '"';
            escaped += c;
        }
        escaped += '"';
        return escaped;
    }
    return s;
}

bool is_blank(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
}

std::expected<double, std::string> get_optional_non_negative_double(const nlohmann::json& j,
                                                                    const char* key,
                                                                    double fallback) {
    if (!j.contains(key)) return fallback;
    if (!j.at(key).is_number()) {
        return std::unexpected(fmt::format("'{}' must be a number", key));
    }
    const double value = j.at(key).get<double>();
    if (!std::isfinite(value) || value < 0.0) {
        return std::unexpected(fmt::format("'{}' must be finite and non-negative", key));
    }
    return value;
}

std::expected<size_t, std::string> get_optional_count(const nlohmann::json& j,
                                                      const char* key,
                                                      size_t fallback) {
    if (!j.contains(key)) return fallback;
    const auto& value = j.at(key);
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return std::unexpected(fmt::format("'{}' must be an integer count", key));
    }
    if (value.is_number_integer() && value.get<std::int64_t>() < 0) {
        return std::unexpected(fmt::format("'{}' must be non-negative", key));
    }
    return value.get<size_t>();
}

std::expected<std::string, std::string> get_optional_string(const nlohmann::json& j,
                                                            const char* key,
                                                            std::string fallback = {}) {
    if (!j.contains(key)) return fallback;
    const auto& value = j.at(key);
    if (!value.is_string()) {
        return std::unexpected(fmt::format("'{}' must be a string", key));
    }
    return value.get<std::string>();
}

std::expected<std::string, std::string> get_required_nonblank_string(const nlohmann::json& j,
                                                                     const char* key) {
    auto value = get_optional_string(j, key);
    if (!value.has_value()) {
        return value;
    }
    if (value->empty() || is_blank(*value)) {
        return std::unexpected(fmt::format("'{}' must be a non-empty string", key));
    }
    return value;
}

std::expected<bool, std::string> get_optional_bool(const nlohmann::json& j,
                                                   const char* key,
                                                   bool fallback) {
    if (!j.contains(key)) return fallback;
    const auto& value = j.at(key);
    if (!value.is_boolean()) {
        return std::unexpected(fmt::format("'{}' must be a boolean", key));
    }
    return value.get<bool>();
}

std::expected<nlohmann::json, std::string> load_report_json(const std::string& run_id) {
    namespace fs = std::filesystem;

    fs::path runs_dir;
    try {
        runs_dir = fs::current_path() / "runs";

        if (!fs::exists(runs_dir)) {
            return std::unexpected("No runs directory found in current path");
        }

        std::vector<std::pair<std::string, fs::path>> matches;
        for (const auto& entry : fs::directory_iterator(runs_dir)) {
            if (!entry.is_directory()) continue;
            std::string dirname = entry.path().filename().string();
            if (dirname.find(run_id) != std::string::npos) {
                fs::path report_file = entry.path() / "report.json";
                if (fs::exists(report_file)) {
                    matches.emplace_back(dirname, report_file);
                }
            }
        }

        std::sort(matches.begin(), matches.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first > rhs.first;
        });

        if (!matches.empty()) {
            const fs::path& report_file = matches.front().second;
            std::ifstream f(report_file);
            if (!f.is_open()) {
                return std::unexpected(fmt::format("Cannot open {}", report_file.string()));
            }
            nlohmann::json j;
            try {
                f >> j;
            } catch (const nlohmann::json::exception& e) {
                return std::unexpected(fmt::format("Invalid JSON in {}: {}", report_file.string(), e.what()));
            }
            return j;
        }
    } catch (const fs::filesystem_error& e) {
        return std::unexpected(fmt::format("Cannot inspect runs directory: {}", e.what()));
    }

    return std::unexpected(fmt::format("Run '{}' not found", run_id));
}

std::expected<std::string, std::string> latest_run_dir_with_artifact(const std::filesystem::path& runs_dir,
                                                                     const char* artifact_name) {
    namespace fs = std::filesystem;

    if (!fs::exists(runs_dir)) {
        return std::unexpected("No runs directory found");
    }

    std::vector<std::string> matches;
    for (const auto& entry : fs::directory_iterator(runs_dir)) {
        if (!entry.is_directory()) continue;
        if (!fs::exists(entry.path() / artifact_name)) continue;
        matches.push_back(entry.path().filename().string());
    }

    std::sort(matches.begin(), matches.end(), std::greater<>());
    if (matches.empty()) {
        return std::unexpected(fmt::format("No runs with {} found", artifact_name));
    }
    return matches.front();
}

} // namespace

int cmd_report_show(const CliOptions& opts, const std::string& run_id) {
    if (is_blank(run_id)) {
        fmt::print(stderr, "Error: run id must not be empty\n");
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    auto result = load_report_json(run_id);
    if (!result.has_value()) {
        fmt::print(stderr, "Error: {}\n", result.error());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    reporting::Report report;
    const auto& j = result.value();
    try {
        report = reporting::Report::from_json(j);
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: Invalid report JSON: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    if (opts.json_output) {
        fmt::print("{}\n", j.dump(2));
        return 0;
    }

    fmt::print("Scenario: {}\n", report.scenario_name);
    fmt::print("Status:   {}\n", report.status);
    fmt::print("Hash:     {}\n", report.scenario_hash);
    fmt::print("Planned start: {:.3f}s\n", report.planned_start_sec);
    fmt::print("Actual start:  {:.3f}s\n", report.actual_start_sec);
    fmt::print("Actual stop:   {:.3f}s\n", report.actual_stop_sec);
    fmt::print("Duration:      {:.3f}s\n", report.actual_duration_sec);
    fmt::print("\n");

    if (!report.devices.empty()) {
        fmt::print("Devices ({}):\n", report.devices.size());
        for (const auto& d : report.devices) {
            fmt::print("  {} ({}) ch{}\n", d.device_id, d.device_type, d.channel);
        }
        fmt::print("\n");
    }

    if (!report.warnings.empty()) {
        fmt::print("Warnings ({}):\n", report.warnings.size());
        for (const auto& w : report.warnings) {
            fmt::print("  [{}] {} — {}\n",
                       common::category_to_string(w.category), w.code, w.message);
        }
        fmt::print("\n");
    }

    if (!report.errors.empty()) {
        fmt::print("Errors ({}):\n", report.errors.size());
        for (const auto& e : report.errors) {
            fmt::print("  [{}] {} — {}\n",
                       common::category_to_string(e.category), e.code, e.message);
        }
        fmt::print("\n");
    }

    if (!report.artifact_paths.empty()) {
        fmt::print("Artifacts ({}):\n", report.artifact_paths.size());
        for (const auto& p : report.artifact_paths) {
            fmt::print("  {}\n", p);
        }
    }

    return 0;
}

int cmd_metrics_export(const CliOptions& opts, const std::string& run_id, const std::string& output_path) {
    (void)opts;
    if (is_blank(run_id)) {
        fmt::print(stderr, "Error: run id must not be empty\n");
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    if (output_path.empty()) {
        fmt::print(stderr, "Error: output path must not be empty\n");
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    auto result = load_report_json(run_id);
    if (!result.has_value()) {
        fmt::print(stderr, "Error: {}\n", result.error());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    const auto& report_json = result.value();
    reporting::Report report;
    try {
        report = reporting::Report::from_json(report_json);
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: Invalid report JSON: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    reporting::ScenarioMetricsSummary summary;
    summary.total_emitters = 0;
    summary.completed_emitters = 0;
    summary.scenario_start_sec = report.planned_start_sec;
    summary.scenario_end_sec = report.actual_stop_sec;
    summary.total_duration_sec = report.actual_duration_sec;

    try {
        if (report_json.contains("emitters")) {
            if (!report_json["emitters"].is_array()) {
                fmt::print(stderr, "Error: Invalid emitter metrics in report JSON: 'emitters' must be an array\n");
                return static_cast<int>(ExitCode::GenericFailure);
            }
            for (const auto& ej : report_json["emitters"]) {
                if (!ej.is_object()) {
                    fmt::print(stderr, "Error: Invalid emitter metrics in report JSON: emitter entries must be objects\n");
                    return static_cast<int>(ExitCode::GenericFailure);
                }
                reporting::EmitterMetrics m;
                auto emitter_id = get_required_nonblank_string(ej, "emitter_id");
                auto device_id = get_optional_string(ej, "device_id");
                auto start_time_sec = get_optional_non_negative_double(ej, "start_time_sec", 0.0);
                auto duration_sec = get_optional_non_negative_double(ej, "duration_sec", 0.0);
                auto samples_rendered = get_optional_count(ej, "samples_rendered", size_t{0});
                auto peak_amplitude = get_optional_non_negative_double(ej, "peak_amplitude", 0.0);
                auto rms_amplitude = get_optional_non_negative_double(ej, "rms_amplitude", 0.0);
                auto crest_factor = get_optional_non_negative_double(ej, "crest_factor", 0.0);
                auto nominal_bandwidth = get_optional_non_negative_double(ej, "nominal_bandwidth", 0.0);
                auto waveform_type = get_optional_string(ej, "waveform_type");
                auto completed = get_optional_bool(ej, "completed", false);
                if (!emitter_id.has_value() || !device_id.has_value() ||
                    !start_time_sec.has_value() || !duration_sec.has_value() || !samples_rendered.has_value() ||
                    !peak_amplitude.has_value() || !rms_amplitude.has_value() || !crest_factor.has_value() ||
                    !nominal_bandwidth.has_value() || !waveform_type.has_value() || !completed.has_value()) {
                    const std::string error =
                        !emitter_id.has_value() ? emitter_id.error() :
                        !device_id.has_value() ? device_id.error() :
                        !start_time_sec.has_value() ? start_time_sec.error() :
                        !duration_sec.has_value() ? duration_sec.error() :
                        !samples_rendered.has_value() ? samples_rendered.error() :
                        !peak_amplitude.has_value() ? peak_amplitude.error() :
                        !rms_amplitude.has_value() ? rms_amplitude.error() :
                        !crest_factor.has_value() ? crest_factor.error() :
                        !nominal_bandwidth.has_value() ? nominal_bandwidth.error() :
                        !waveform_type.has_value() ? waveform_type.error() :
                        completed.error();
                    fmt::print(stderr, "Error: Invalid emitter metrics in report JSON: {}\n", error);
                    return static_cast<int>(ExitCode::GenericFailure);
                }
                m.emitter_id = *emitter_id;
                m.device_id = *device_id;
                m.start_time_sec = *start_time_sec;
                m.duration_sec = *duration_sec;
                m.samples_rendered = *samples_rendered;
                m.peak_amplitude = *peak_amplitude;
                m.rms_amplitude = *rms_amplitude;
                m.crest_factor = *crest_factor;
                m.nominal_bandwidth = *nominal_bandwidth;
                m.waveform_type = *waveform_type;
                m.completed = *completed;
                if (summary.emitters.find(m.emitter_id) != summary.emitters.end()) {
                    fmt::print(stderr,
                               "Error: Invalid emitter metrics in report JSON: duplicate emitter_id '{}'\n",
                               m.emitter_id);
                    return static_cast<int>(ExitCode::GenericFailure);
                }
                summary.emitters[m.emitter_id] = m;
                summary.total_emitters++;
                if (m.completed) summary.completed_emitters++;
            }
        }
    } catch (const nlohmann::json::exception& e) {
        fmt::print(stderr, "Error: Invalid emitter metrics in report JSON: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    bool is_csv = output_path.size() >= 4 && output_path.substr(output_path.size() - 4) == ".csv";

    if (is_csv) {
        std::ofstream f(output_path);
        if (!f.is_open()) {
            fmt::print(stderr, "Error: Cannot open '{}' for writing\n", output_path);
            return static_cast<int>(ExitCode::GenericFailure);
        }
        f << "emitter_id,device_id,start_time_sec,duration_sec,samples_rendered,"
          << "peak_amplitude,rms_amplitude,crest_factor,nominal_bandwidth,"
          << "waveform_type,completed\n";
        for (const auto& [id, m] : summary.emitters) {
            f << csv_escape(m.emitter_id) << "," << csv_escape(m.device_id) << ","
              << m.start_time_sec << "," << m.duration_sec << ","
              << m.samples_rendered << "," << m.peak_amplitude << ","
              << m.rms_amplitude << "," << m.crest_factor << ","
              << m.nominal_bandwidth << "," << csv_escape(m.waveform_type) << ","
              << (m.completed ? "true" : "false") << "\n";
        }
        f.close();
        if (!f) {
            fmt::print(stderr, "Error: Failed while writing '{}'\n", output_path);
            return static_cast<int>(ExitCode::GenericFailure);
        }
    } else {
        auto j = reporting::to_json(summary);
        std::ofstream f(output_path);
        if (!f.is_open()) {
            fmt::print(stderr, "Error: Cannot open '{}' for writing\n", output_path);
            return static_cast<int>(ExitCode::GenericFailure);
        }
        f << j.dump(2) << std::endl;
        f.close();
        if (!f) {
            fmt::print(stderr, "Error: Failed while writing '{}'\n", output_path);
            return static_cast<int>(ExitCode::GenericFailure);
        }
    }

    fmt::print("Metrics exported to {}\n", output_path);
    return 0;
}

int cmd_metrics_export_latest(const CliOptions& opts, const std::string& output_path) {
    namespace fs = std::filesystem;

    try {
        fs::path runs_dir = fs::current_path() / "runs";
        auto latest_dir = latest_run_dir_with_artifact(runs_dir, "report.json");
        if (!latest_dir.has_value()) {
            fmt::print(stderr, "Error: {}\n", latest_dir.error());
            return static_cast<int>(ExitCode::GenericFailure);
        }

        return cmd_metrics_export(opts, *latest_dir, output_path);
    } catch (const fs::filesystem_error& e) {
        fmt::print(stderr, "Error: Cannot inspect runs directory: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }
}

int cmd_metrics_export_latest_ex(const CliOptions& opts,
                                 const std::string& output_path,
                                 const std::string& format) {
    (void)opts;
    namespace fs = std::filesystem;

    if (format != "json" && format != "csv") {
        fmt::print(stderr, "Error: Unsupported metrics export format '{}'\n", format);
        return static_cast<int>(ExitCode::GenericFailure);
    }

    try {
        fs::path runs_dir = fs::current_path() / "runs";
        auto latest_dir = latest_run_dir_with_artifact(runs_dir, "metrics.json");
        if (!latest_dir.has_value()) {
            fmt::print(stderr, "Error: {}\n", latest_dir.error());
            return static_cast<int>(ExitCode::GenericFailure);
        }

        fs::path metrics_file = runs_dir / *latest_dir / "metrics.json";

        std::ifstream f(metrics_file);
        if (!f.is_open()) {
            fmt::print(stderr, "Error: Cannot open {}\n", metrics_file.string());
            return static_cast<int>(ExitCode::GenericFailure);
        }

        nlohmann::json metrics_json;
        try {
            f >> metrics_json;
        } catch (const nlohmann::json::exception& e) {
            fmt::print(stderr, "Error: Invalid JSON in {}: {}\n", metrics_file.string(), e.what());
            return static_cast<int>(ExitCode::GenericFailure);
        }

        try {
            (void)reporting::Metrics::from_json(metrics_json);
        } catch (const std::exception& e) {
            fmt::print(stderr, "Error: Invalid metrics JSON in {}: {}\n", metrics_file.string(), e.what());
            return static_cast<int>(ExitCode::GenericFailure);
        }

        std::string output;
        if (format == "csv") {
            std::vector<std::pair<std::string, std::string>> rows;
            flatten_json(metrics_json, "", rows);

            output = "key,value\n";
            for (const auto& [k, v] : rows) {
                output += csv_escape(k) + "," + csv_escape(v) + "\n";
            }
        } else {
            output = metrics_json.dump(2) + "\n";
        }

        if (output_path.empty()) {
            fmt::print("{}", output);
        } else {
            std::ofstream out(output_path);
            if (!out.is_open()) {
                fmt::print(stderr, "Error: Cannot open '{}' for writing\n", output_path);
                return static_cast<int>(ExitCode::GenericFailure);
            }
            out << output;
            out.close();
            if (!out) {
                fmt::print(stderr, "Error: Failed while writing '{}'\n", output_path);
                return static_cast<int>(ExitCode::GenericFailure);
            }
            fmt::print("Metrics exported to {}\n", output_path);
        }
    } catch (const fs::filesystem_error& e) {
        fmt::print(stderr, "Error: Cannot inspect runs directory: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    return 0;
}

} // namespace archerfish::cli
