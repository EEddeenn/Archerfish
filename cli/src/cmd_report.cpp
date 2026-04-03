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

} // namespace archerfish::cli
