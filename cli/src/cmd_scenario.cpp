#include "archerfish/cli/cmd_scenario.hpp"
#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"
#include "archerfish/scenario/plan_io.hpp"
#include "archerfish/runtime/runtime.hpp"
#include "archerfish/hal/hal_factory.hpp"
#include "archerfish/reporting/run_directory.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace archerfish::cli {

namespace {

bool is_blank(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
}

void print_errors(const common::ErrorList& errors) {
    for (const auto& e : errors) {
        fmt::print(stderr, "  [{}] {} — {}\n", common::category_to_string(e.category), e.code, e.message);
    }
}

reporting::Metrics to_reporting_metrics(const runtime::Runtime::RunMetrics& metrics) {
    reporting::Metrics out;
    out.start_requested_sec = metrics.actual_start_sec;
    out.start_actual_sec = metrics.actual_start_sec;
    out.stop_actual_sec = metrics.actual_stop_sec;
    out.tx_duration_sec = metrics.actual_duration_sec;
    out.underrun_count = metrics.underruns;
    out.total_samples_sent = metrics.total_samples_sent;
    return out;
}

reporting::Report make_run_report(const scenario::Scenario& scenario,
                                   const scenario::Plan& plan,
                                   const runtime::Runtime::RunMetrics& metrics,
                                   const std::string& target_device_id,
                                   const std::vector<runtime::MarkerDispatch>& markers,
                                   const std::vector<std::string>& artifact_paths) {
    reporting::Report report;
    report.scenario_name = scenario.metadata.name;
    report.status = "completed";
    report.planned_start_sec = 0.0;
    report.actual_start_sec = metrics.actual_start_sec;
    report.actual_stop_sec = metrics.actual_stop_sec;
    report.actual_duration_sec = metrics.actual_duration_sec;
    report.artifact_paths = artifact_paths;

    const std::string device_type = target_device_id == "stub0" ? "stub" : "usrp";
    for (const auto& ch : plan.channels) {
        report.devices.push_back({ch.device_id, device_type, ch.channel_index});
    }
    for (const auto& marker : markers) {
        report.marker_events.push_back({marker.name, marker.planned_time_sec, marker.wall_clock_sec});
    }
    return report;
}

class ScopedSpdlogLevel {
public:
    ScopedSpdlogLevel(bool active, spdlog::level::level_enum level) : logger_(spdlog::default_logger()), active_(active && logger_) {
        if (active_) {
            previous_ = logger_->level();
            logger_->set_level(level);
        }
    }

    ~ScopedSpdlogLevel() {
        if (active_) {
            logger_->set_level(previous_);
        }
    }

    ScopedSpdlogLevel(const ScopedSpdlogLevel&) = delete;
    ScopedSpdlogLevel& operator=(const ScopedSpdlogLevel&) = delete;

private:
    std::shared_ptr<spdlog::logger> logger_;
    spdlog::level::level_enum previous_{spdlog::level::info};
    bool active_{false};
};

} // namespace

int cmd_scenario_validate(const CliOptions& opts, const std::string& file_path) {
    auto parse_result = scenario::parse_scenario(file_path);
    if (!parse_result.has_value()) {
        fmt::print(stderr, "Parse errors:\n");
        print_errors(parse_result.error());
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    auto& scenario = parse_result.value();
    auto ref_errors = scenario::resolve_waveform_refs(scenario);
    if (!ref_errors.empty()) {
        fmt::print(stderr, "Reference resolution errors:\n");
        print_errors(ref_errors);
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    auto result = scenario::validate(scenario);

    if (opts.json_output) {
        nlohmann::json out;
        out["valid"] = result.ok();
        out["errors"] = nlohmann::json::array();
        for (const auto& e : result.errors) {
            out["errors"].push_back({{"category", common::category_to_string(e.category)}, {"code", e.code}, {"message", e.message}});
        }
        out["warnings"] = nlohmann::json::array();
        for (const auto& w : result.warnings) {
            out["warnings"].push_back({{"category", common::category_to_string(w.category)}, {"code", w.code}, {"message", w.message}});
        }
        fmt::print("{}\n", out.dump(2));
    } else {
        if (result.ok()) {
            fmt::print("Validation: PASSED\n");
        } else {
            fmt::print(stderr, "Validation: FAILED\n");
        }
        if (!result.errors.empty()) {
            fmt::print(stderr, "Errors:\n");
            print_errors(result.errors);
        }
        if (!result.warnings.empty()) {
            fmt::print("Warnings:\n");
            print_errors(result.warnings);
        }
    }

    return result.ok() ? 0 : static_cast<int>(ExitCode::InputValidationFailure);
}

int cmd_scenario_plan(const CliOptions& opts, const std::string& file_path) {
    auto parse_result = scenario::parse_scenario(file_path);
    if (!parse_result.has_value()) {
        fmt::print(stderr, "Parse errors:\n");
        print_errors(parse_result.error());
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    auto& scenario = parse_result.value();
    auto ref_errors = scenario::resolve_waveform_refs(scenario);
    if (!ref_errors.empty()) {
        fmt::print(stderr, "Reference resolution errors:\n");
        print_errors(ref_errors);
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    auto val_result = scenario::validate(scenario);
    if (!val_result.ok()) {
        fmt::print(stderr, "Validation failed:\n");
        print_errors(val_result.errors);
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    auto plan_result = scenario::plan(scenario);
    if (!plan_result.has_value()) {
        fmt::print(stderr, "Planning errors:\n");
        print_errors(plan_result.error());
        return static_cast<int>(ExitCode::PlanningFailure);
    }

    const auto& plan = plan_result.value();

    if (opts.json_output) {
        fmt::print("{}\n", scenario::plan_to_json(plan).dump(2));
    } else {
        fmt::print("Plan summary:\n");
        fmt::print("  Channels:    {}\n", plan.channels.size());
        fmt::print("  Emitters:    {}\n", plan.render_instructions.size());
        fmt::print("  Timeline:    {} events\n", plan.timeline.size());
        fmt::print("  Duration:    {:.3f} s\n", plan.estimated_duration_sec);
    }

    return 0;
}

int cmd_scenario_run(const CliOptions& opts, const std::string& file_path) {
    ScopedSpdlogLevel quiet_json_logs(opts.json_output, spdlog::level::off);

    auto parse_result = scenario::parse_scenario(file_path);
    if (!parse_result.has_value()) {
        fmt::print(stderr, "Parse errors:\n");
        print_errors(parse_result.error());
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    auto& scenario = parse_result.value();
    auto ref_errors = scenario::resolve_waveform_refs(scenario);
    if (!ref_errors.empty()) {
        fmt::print(stderr, "Reference resolution errors:\n");
        print_errors(ref_errors);
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    auto val_result = scenario::validate(scenario);
    if (!val_result.ok()) {
        fmt::print(stderr, "Validation failed:\n");
        print_errors(val_result.errors);
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    auto plan_result = scenario::plan(scenario);
    if (!plan_result.has_value()) {
        fmt::print(stderr, "Planning errors:\n");
        print_errors(plan_result.error());
        return static_cast<int>(ExitCode::PlanningFailure);
    }

    if (!opts.device_id.empty() && is_blank(opts.device_id)) {
        fmt::print(stderr, "Device id must not be blank.\n");
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    std::string target_device_id;

    if (!opts.device_id.empty()) {
        target_device_id = opts.device_id;
    } else {
        std::vector<hal::DiscoveredDevice> discovered;
        try {
            discovered = hal::discover_devices();
        } catch (const std::exception& e) {
            fmt::print(stderr, "Failed to discover devices: {}\n", e.what());
            return static_cast<int>(ExitCode::GenericFailure);
        }

        for (const auto& d : discovered) {
            if (d.type == "usrp") {
                target_device_id = d.id;
                break;
            }
        }
        if (target_device_id.empty()) {
            target_device_id = "stub0";
            spdlog::warn("No USRP devices found, using stub device");
        }
    }

    std::shared_ptr<hal::IHalDevice> device;
    try {
        device = hal::open_device(target_device_id);
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to open device '{}': {}\n", target_device_id, e.what());
        return static_cast<int>(ExitCode::PreparationFailure);
    }

    if (!opts.json_output) {
        fmt::print("Using device: {} ({})\n", target_device_id,
                   target_device_id == "stub0" ? "stub" : "USRP");
    }

    runtime::Runtime rt(device);

    if (!rt.prepare(plan_result.value())) {
        fmt::print(stderr, "Runtime preparation failed.\n");
        return static_cast<int>(ExitCode::PreparationFailure);
    }

    if (!rt.arm()) {
        fmt::print(stderr, "Runtime arm failed.\n");
        return static_cast<int>(ExitCode::PreparationFailure);
    }

    if (!rt.run()) {
        fmt::print(stderr, "Runtime execution failed.\n");
        return static_cast<int>(ExitCode::ExecutionFailure);
    }

    auto metrics = rt.get_metrics();
    std::string run_dir_path;
    if (scenario.reporting.save_plan || scenario.reporting.save_metrics) {
        try {
            reporting::RunDirectory run_dir(std::filesystem::current_path(), scenario.metadata.name);
            std::vector<std::string> artifact_paths;

            run_dir.save_scenario(scenario);
            artifact_paths.push_back(run_dir.scenario_path().string());

            if (scenario.reporting.save_plan) {
                run_dir.save_plan(plan_result.value());
                artifact_paths.push_back(run_dir.plan_path().string());
            }
            if (scenario.reporting.save_metrics) {
                run_dir.save_metrics(to_reporting_metrics(metrics));
                artifact_paths.push_back(run_dir.metrics_path().string());
            }

            auto report = make_run_report(scenario,
                                          plan_result.value(),
                                          metrics,
                                          target_device_id,
                                          rt.marker_dispatches(),
                                          artifact_paths);
            artifact_paths.push_back(run_dir.report_path().string());
            report.artifact_paths = artifact_paths;
            run_dir.save_report(report);
            run_dir.append_log("Run completed");
            run_dir_path = run_dir.path().string();
        } catch (const std::exception& e) {
            fmt::print(stderr, "Failed to write run artifacts: {}\n", e.what());
            return static_cast<int>(ExitCode::GenericFailure);
        }
    }

    if (opts.json_output) {
        nlohmann::json out = {
            {"device", target_device_id},
            {"samples_sent", metrics.total_samples_sent},
            {"blocks_sent", metrics.total_blocks_sent},
            {"underruns", metrics.underruns},
            {"start_sec", metrics.actual_start_sec},
            {"stop_sec", metrics.actual_stop_sec},
            {"duration_sec", metrics.actual_duration_sec},
        };
        if (!run_dir_path.empty()) {
            out["run_dir"] = run_dir_path;
        }
        fmt::print("{}\n", out.dump(2));
    } else {
        fmt::print("Run complete.\n");
        fmt::print("  Device:       {}\n", target_device_id);
        fmt::print("  Samples sent: {}\n", metrics.total_samples_sent);
        fmt::print("  Blocks sent:  {}\n", metrics.total_blocks_sent);
        fmt::print("  Underruns:    {}\n", metrics.underruns);
        fmt::print("  Duration:     {:.6f} s\n", metrics.actual_duration_sec);
        if (!run_dir_path.empty()) {
            fmt::print("  Run dir:      {}\n", run_dir_path);
        }
    }

    return 0;
}

} // namespace archerfish::cli
