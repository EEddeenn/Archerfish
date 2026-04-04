#include "archerfish/cli/cmd_report.hpp"
#include "archerfish/cli/format.hpp"
#include "archerfish/reporting/report.hpp"
#include "archerfish/reporting/emitter_metrics.hpp"

#include <expected>
#include <filesystem>
#include <fstream>

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

std::expected<nlohmann::json, std::string> load_report_json(const std::string& run_id) {
    namespace fs = std::filesystem;

    fs::path runs_dir = fs::current_path() / "runs";

    if (!fs::exists(runs_dir)) {
        return std::unexpected("No runs directory found in current path");
    }

    for (const auto& entry : fs::directory_iterator(runs_dir)) {
        if (!entry.is_directory()) continue;
        std::string dirname = entry.path().filename().string();
        if (dirname.find(run_id) != std::string::npos) {
            fs::path report_file = entry.path() / "report.json";
            if (fs::exists(report_file)) {
                std::ifstream f(report_file);
                if (!f.is_open()) {
                    return std::unexpected(fmt::format("Cannot open {}", report_file.string()));
                }
                nlohmann::json j;
                f >> j;
                return j;
            }
        }
    }

    return std::unexpected(fmt::format("Run '{}' not found", run_id));
}

} // namespace

int cmd_report_show(const CliOptions& opts, const std::string& run_id) {
    auto result = load_report_json(run_id);
    if (!result.has_value()) {
        fmt::print(stderr, "Error: {}\n", result.error());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    const auto& j = result.value();

    if (opts.json_output) {
        fmt::print("{}\n", j.dump(2));
        return 0;
    }

    auto report = reporting::Report::from_json(j);

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
    auto result = load_report_json(run_id);
    if (!result.has_value()) {
        fmt::print(stderr, "Error: {}\n", result.error());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    const auto& report_json = result.value();
    auto report = reporting::Report::from_json(report_json);

    reporting::ScenarioMetricsSummary summary;
    summary.total_emitters = 0;
    summary.completed_emitters = 0;
    summary.scenario_start_sec = report.planned_start_sec;
    summary.scenario_end_sec = report.actual_stop_sec;
    summary.total_duration_sec = report.actual_duration_sec;

    if (report_json.contains("emitters")) {
        for (const auto& ej : report_json["emitters"]) {
            reporting::EmitterMetrics m;
            m.emitter_id = ej.value("emitter_id", "");
            m.device_id = ej.value("device_id", "");
            m.start_time_sec = ej.value("start_time_sec", 0.0);
            m.duration_sec = ej.value("duration_sec", 0.0);
            m.samples_rendered = ej.value("samples_rendered", size_t{0});
            m.peak_amplitude = ej.value("peak_amplitude", 0.0);
            m.rms_amplitude = ej.value("rms_amplitude", 0.0);
            m.crest_factor = ej.value("crest_factor", 0.0);
            m.nominal_bandwidth = ej.value("nominal_bandwidth", 0.0);
            m.waveform_type = ej.value("waveform_type", "");
            m.completed = ej.value("completed", false);
            summary.emitters[m.emitter_id] = m;
            summary.total_emitters++;
            if (m.completed) summary.completed_emitters++;
        }
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
            f << m.emitter_id << "," << m.device_id << ","
              << m.start_time_sec << "," << m.duration_sec << ","
              << m.samples_rendered << "," << m.peak_amplitude << ","
              << m.rms_amplitude << "," << m.crest_factor << ","
              << m.nominal_bandwidth << "," << m.waveform_type << ","
              << (m.completed ? "true" : "false") << "\n";
        }
        f.close();
    } else {
        auto j = reporting::to_json(summary);
        std::ofstream f(output_path);
        if (!f.is_open()) {
            fmt::print(stderr, "Error: Cannot open '{}' for writing\n", output_path);
            return static_cast<int>(ExitCode::GenericFailure);
        }
        f << j.dump(2) << std::endl;
        f.close();
    }

    fmt::print("Metrics exported to {}\n", output_path);
    return 0;
}

int cmd_metrics_export_latest(const CliOptions& opts, const std::string& output_path) {
    namespace fs = std::filesystem;

    fs::path runs_dir = fs::current_path() / "runs";
    if (!fs::exists(runs_dir)) {
        fmt::print(stderr, "Error: No runs directory found\n");
        return static_cast<int>(ExitCode::GenericFailure);
    }

    std::string latest_dir;
    std::filesystem::file_time_type latest_time{};
    bool found = false;

    for (const auto& entry : fs::directory_iterator(runs_dir)) {
        if (!entry.is_directory()) continue;
        if (!found || entry.last_write_time() > latest_time) {
            latest_time = entry.last_write_time();
            latest_dir = entry.path().filename().string();
            found = true;
        }
    }

    if (!found) {
        fmt::print(stderr, "Error: No runs found\n");
        return static_cast<int>(ExitCode::GenericFailure);
    }

    return cmd_metrics_export(opts, latest_dir, output_path);
}

int cmd_metrics_export_latest_ex(const CliOptions& opts,
                                 const std::string& output_path,
                                 const std::string& format) {
    (void)opts;
    namespace fs = std::filesystem;

    fs::path runs_dir = fs::current_path() / "runs";
    if (!fs::exists(runs_dir)) {
        fmt::print(stderr, "Error: No runs directory found\n");
        return static_cast<int>(ExitCode::GenericFailure);
    }

    std::string latest_dir;
    fs::file_time_type latest_time{};
    bool found = false;

    for (const auto& entry : fs::directory_iterator(runs_dir)) {
        if (!entry.is_directory()) continue;
        if (!found || entry.last_write_time() > latest_time) {
            latest_time = entry.last_write_time();
            latest_dir = entry.path().filename().string();
            found = true;
        }
    }

    if (!found) {
        fmt::print(stderr, "Error: No runs found\n");
        return static_cast<int>(ExitCode::GenericFailure);
    }

    fs::path metrics_file = runs_dir / latest_dir / "metrics.json";
    if (!fs::exists(metrics_file)) {
        fmt::print(stderr, "Error: metrics.json not found in run '{}'\n", latest_dir);
        return static_cast<int>(ExitCode::GenericFailure);
    }

    std::ifstream f(metrics_file);
    if (!f.is_open()) {
        fmt::print(stderr, "Error: Cannot open {}\n", metrics_file.string());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    nlohmann::json metrics_json;
    f >> metrics_json;

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
        fmt::print("Metrics exported to {}\n", output_path);
    }

    return 0;
}

} // namespace archerfish::cli
