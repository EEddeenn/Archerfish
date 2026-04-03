#include <archerfish/cli/app.hpp>
#include <archerfish/cli/cmd_devices.hpp>
#include <archerfish/cli/cmd_scenario.hpp>
#include <archerfish/cli/cmd_wave.hpp>
#include <archerfish/cli/cmd_doctor.hpp>

#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <functional>
#include <string>

namespace archerfish::cli {

namespace {

struct CommandRegistry {
    std::string file_path;
    std::string device_id;
    std::string output_path;

    double rate{0.0};
    double duration{0.0};
    double amplitude{0.2};
    double f0{0.0};
    double f1{0.0};
    double symbol_rate{0.0};
    int sps{4};
    double rrc_alpha{0.35};

    std::function<int()> execute;
};

} // namespace

std::shared_ptr<CLI::App> build_app(CliOptions& opts) {
    auto app = std::make_shared<CLI::App>("archerfish — programmable vector signal generator", "archerfish");
    app->set_version_flag("--version", "archerfish v0.1.0");

    auto reg = std::make_shared<CommandRegistry>();
    app->preparse_callback([reg](size_t) {});

    app->set_config("--config", "", "Read config from an ini file", false);

    // --- devices subcommand ---
    auto* devices = app->add_subcommand("devices", "Device management");

    auto* devices_list = devices->add_subcommand("list", "List available devices");
    devices_list->callback([&opts]() { return cmd_devices_list(opts); });

    auto* devices_info = devices->add_subcommand("info", "Show device info");
    devices_info->add_option("--device", reg->device_id, "Device ID")->required();
    devices_info->callback([&opts, &r = *reg]() { return cmd_devices_info(opts, r.device_id); });

    // --- scenario subcommand ---
    auto* scenario = app->add_subcommand("scenario", "Scenario operations");

    auto* scenario_validate = scenario->add_subcommand("validate", "Validate a scenario file");
    scenario_validate->add_option("file", reg->file_path, "Scenario JSON file")->required()->check(CLI::ExistingFile);
    scenario_validate->callback([&opts, &r = *reg]() { return cmd_scenario_validate(opts, r.file_path); });

    auto* scenario_plan = scenario->add_subcommand("plan", "Plan a scenario");
    scenario_plan->add_option("file", reg->file_path, "Scenario JSON file")->required()->check(CLI::ExistingFile);
    scenario_plan->add_flag("--json", opts.json_output, "Output as JSON");
    scenario_plan->callback([&opts, &r = *reg]() { return cmd_scenario_plan(opts, r.file_path); });

    auto* scenario_run = scenario->add_subcommand("run", "Run a scenario");
    scenario_run->add_option("file", reg->file_path, "Scenario JSON file")->required()->check(CLI::ExistingFile);
    scenario_run->add_flag("--json", opts.json_output, "Output as JSON");
    scenario_run->add_option("--device", opts.device_id, "Device ID (auto-detect if omitted)");
    scenario_run->callback([&opts, &r = *reg]() { return cmd_scenario_run(opts, r.file_path); });

    // --- wave subcommand ---
    auto* wave = app->add_subcommand("wave", "Waveform generation and inspection");

    auto* wave_gen = wave->add_subcommand("gen", "Generate waveforms");

    auto* wave_gen_cw = wave_gen->add_subcommand("cw", "Generate CW waveform");
    wave_gen_cw->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_cw->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_cw->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_cw->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_cw->callback([&opts, &r = *reg]() { return cmd_wave_gen_cw(opts, r.rate, r.duration, r.amplitude, r.output_path); });

    auto* wave_gen_chirp = wave_gen->add_subcommand("chirp", "Generate chirp waveform");
    wave_gen_chirp->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_chirp->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_chirp->add_option("--f0", reg->f0, "Start frequency (Hz)")->required();
    wave_gen_chirp->add_option("--f1", reg->f1, "End frequency (Hz)")->required();
    wave_gen_chirp->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_chirp->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_chirp->callback([&opts, &r = *reg]() { return cmd_wave_gen_chirp(opts, r.rate, r.duration, r.f0, r.f1, r.amplitude, r.output_path); });

    auto* wave_gen_qpsk = wave_gen->add_subcommand("qpsk", "Generate QPSK waveform");
    wave_gen_qpsk->add_option("--symbol-rate", reg->symbol_rate, "Symbol rate (Hz)")->required();
    wave_gen_qpsk->add_option("--sps", reg->sps, "Samples per symbol")->default_val("4");
    wave_gen_qpsk->add_option("--rrc", reg->rrc_alpha, "RRC roll-off factor")->default_val("0.35");
    wave_gen_qpsk->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_qpsk->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_qpsk->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_qpsk->callback([&opts, &r = *reg]() { return cmd_wave_gen_qpsk(opts, r.symbol_rate, r.sps, r.rrc_alpha, r.duration, r.amplitude, r.output_path); });

    auto* wave_inspect = wave->add_subcommand("inspect", "Inspect a waveform file");
    wave_inspect->add_option("file", reg->file_path, "Waveform file")->required()->check(CLI::ExistingFile);
    wave_inspect->add_flag("--json", opts.json_output, "Output as JSON");
    wave_inspect->callback([&opts, &r = *reg]() { return cmd_wave_inspect(opts, r.file_path); });

    // --- version subcommand ---
    auto* version_cmd = app->add_subcommand("version", "Show version");
    version_cmd->callback([&opts]() { return cmd_version(opts); });

    // --- doctor subcommand ---
    auto* doctor_cmd = app->add_subcommand("doctor", "Run diagnostics");
    doctor_cmd->callback([&opts]() { return cmd_doctor(opts); });

    app->require_subcommand(1);

    return app;
}

int run(int argc, char* argv[]) {
    CliOptions opts;
    auto app = build_app(opts);

    try {
        app->parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app->exit(e);
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }

    return static_cast<int>(ExitCode::Success);
}

} // namespace archerfish::cli
