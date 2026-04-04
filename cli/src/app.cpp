#include "archerfish/cli/app.hpp"
#include "archerfish/cli/cmd_devices.hpp"
#include "archerfish/cli/cmd_scenario.hpp"
#include "archerfish/cli/cmd_wave.hpp"
#include "archerfish/cli/cmd_doctor.hpp"
#include "archerfish/cli/cmd_report.hpp"
#include "archerfish/cli/cmd_dryrun.hpp"
#include "archerfish/cli/cmd_schema.hpp"
#include "archerfish/cli/cmd_calib.hpp"

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
    std::string run_id;

    double rate{0.0};
    double duration{0.0};
    double amplitude{0.2};
    double f0{0.0};
    double f1{0.0};
    double symbol_rate{0.0};
    int sps{4};
    double rrc_alpha{0.35};

    double frequency{0.0};
    double pulse_width{1e-6};
    double pri{10e-6};
    int num_levels{2};
    double center_freq{0.0};
    int modulation_order{2};
    double deviation{5000.0};
    double carrier_freq{0.0};
    double mod_freq{1000.0};
    double mod_depth{0.5};
    double mod_index{1.0};

    int fft_size{64};
    int cp_size{16};
    int active_subcarriers{60};

    std::string format{"json"};
    bool metrics_latest{false};

    uint32_t channel{0};
    bool schema_json{false};
    bool schema_markdown{false};
    bool calib_all_channels{false};

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
    devices_list->callback([&opts]() { opts.last_exit_code = cmd_devices_list(opts); });

    auto* devices_info = devices->add_subcommand("info", "Show device info");
    devices_info->add_option("--device", reg->device_id, "Device ID")->required();
    devices_info->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_devices_info(opts, r.device_id); });

    // --- scenario subcommand ---
    auto* scenario = app->add_subcommand("scenario", "Scenario operations");

    auto* scenario_validate = scenario->add_subcommand("validate", "Validate a scenario file");
    scenario_validate->add_option("file", reg->file_path, "Scenario JSON file")->required()->check(CLI::ExistingFile);
    scenario_validate->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_scenario_validate(opts, r.file_path); });

    auto* scenario_plan = scenario->add_subcommand("plan", "Plan a scenario");
    scenario_plan->add_option("file", reg->file_path, "Scenario JSON file")->required()->check(CLI::ExistingFile);
    scenario_plan->add_flag("--json", opts.json_output, "Output as JSON");
    scenario_plan->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_scenario_plan(opts, r.file_path); });

    auto* scenario_run = scenario->add_subcommand("run", "Run a scenario");
    scenario_run->add_option("file", reg->file_path, "Scenario JSON file")->required()->check(CLI::ExistingFile);
    scenario_run->add_flag("--json", opts.json_output, "Output as JSON");
    scenario_run->add_option("--device", opts.device_id, "Device ID (auto-detect if omitted)");
    scenario_run->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_scenario_run(opts, r.file_path); });

    // --- wave subcommand ---
    auto* wave = app->add_subcommand("wave", "Waveform generation and inspection");

    auto* wave_gen = wave->add_subcommand("gen", "Generate waveforms");

    auto* wave_gen_cw = wave_gen->add_subcommand("cw", "Generate CW waveform");
    wave_gen_cw->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_cw->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_cw->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_cw->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_cw->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_cw(opts, r.rate, r.duration, r.amplitude, r.output_path); });

    auto* wave_gen_chirp = wave_gen->add_subcommand("chirp", "Generate chirp waveform");
    wave_gen_chirp->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_chirp->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_chirp->add_option("--f0", reg->f0, "Start frequency (Hz)")->required();
    wave_gen_chirp->add_option("--f1", reg->f1, "End frequency (Hz)")->required();
    wave_gen_chirp->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_chirp->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_chirp->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_chirp(opts, r.rate, r.duration, r.f0, r.f1, r.amplitude, r.output_path); });

    auto* wave_gen_qpsk = wave_gen->add_subcommand("qpsk", "Generate QPSK waveform");
    wave_gen_qpsk->add_option("--symbol-rate", reg->symbol_rate, "Symbol rate (Hz)")->required();
    wave_gen_qpsk->add_option("--sps", reg->sps, "Samples per symbol")->default_val("4");
    wave_gen_qpsk->add_option("--rrc", reg->rrc_alpha, "RRC roll-off factor")->default_val("0.35");
    wave_gen_qpsk->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_qpsk->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_qpsk->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_qpsk->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_qpsk(opts, r.symbol_rate, r.sps, r.rrc_alpha, r.duration, r.amplitude, r.output_path); });

    auto* wave_gen_pulse = wave_gen->add_subcommand("pulse", "Generate pulse waveform");
    wave_gen_pulse->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_pulse->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_pulse->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_pulse->add_option("--frequency", reg->frequency, "Center frequency (Hz)")->default_val("0");
    wave_gen_pulse->add_option("--pulse-width", reg->pulse_width, "Pulse width (sec)")->default_val("1e-6");
    wave_gen_pulse->add_option("--pri", reg->pri, "Pulse repetition interval (sec)")->default_val("10e-6");
    wave_gen_pulse->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_pulse->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_pulse(opts, r.rate, r.duration, r.amplitude, r.frequency, r.pulse_width, r.pri, r.output_path); });

    auto* wave_gen_ask = wave_gen->add_subcommand("ask", "Generate ASK waveform");
    wave_gen_ask->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_ask->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_ask->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_ask->add_option("--frequency", reg->frequency, "Carrier frequency (Hz)")->default_val("0");
    wave_gen_ask->add_option("--symbol-rate", reg->symbol_rate, "Symbol rate (Hz)")->default_val("1e6");
    wave_gen_ask->add_option("--num-levels", reg->num_levels, "Number of amplitude levels")->default_val("2");
    wave_gen_ask->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_ask->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_ask(opts, r.rate, r.duration, r.amplitude, r.frequency, r.symbol_rate, r.num_levels, r.output_path); });

    auto* wave_gen_fsk = wave_gen->add_subcommand("fsk", "Generate FSK waveform");
    wave_gen_fsk->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_fsk->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_fsk->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_fsk->add_option("--center-freq", reg->center_freq, "Center frequency (Hz)")->default_val("0");
    wave_gen_fsk->add_option("--symbol-rate", reg->symbol_rate, "Symbol rate (Hz)")->default_val("1e6");
    wave_gen_fsk->add_option("--modulation-order", reg->modulation_order, "Modulation order (M)")->default_val("2");
    wave_gen_fsk->add_option("--deviation", reg->deviation, "Frequency deviation (Hz)")->default_val("5000");
    wave_gen_fsk->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_fsk->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_fsk(opts, r.rate, r.duration, r.amplitude, r.center_freq, r.symbol_rate, r.modulation_order, r.deviation, r.output_path); });

    auto* wave_gen_am = wave_gen->add_subcommand("am", "Generate AM waveform");
    wave_gen_am->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_am->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_am->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_am->add_option("--carrier-freq", reg->carrier_freq, "Carrier frequency (Hz)")->default_val("0");
    wave_gen_am->add_option("--mod-freq", reg->mod_freq, "Modulation frequency (Hz)")->default_val("1000");
    wave_gen_am->add_option("--mod-depth", reg->mod_depth, "Modulation depth")->default_val("0.5");
    wave_gen_am->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_am->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_am(opts, r.rate, r.duration, r.amplitude, r.carrier_freq, r.mod_freq, r.mod_depth, r.output_path); });

    auto* wave_gen_fm = wave_gen->add_subcommand("fm", "Generate FM waveform");
    wave_gen_fm->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_fm->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_fm->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_fm->add_option("--carrier-freq", reg->carrier_freq, "Carrier frequency (Hz)")->default_val("0");
    wave_gen_fm->add_option("--mod-freq", reg->mod_freq, "Modulation frequency (Hz)")->default_val("1000");
    wave_gen_fm->add_option("--deviation", reg->deviation, "Frequency deviation (Hz)")->default_val("5000");
    wave_gen_fm->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_fm->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_fm(opts, r.rate, r.duration, r.amplitude, r.carrier_freq, r.mod_freq, r.deviation, r.output_path); });

    auto* wave_gen_pm = wave_gen->add_subcommand("pm", "Generate PM waveform");
    wave_gen_pm->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_pm->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_pm->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_pm->add_option("--carrier-freq", reg->carrier_freq, "Carrier frequency (Hz)")->default_val("0");
    wave_gen_pm->add_option("--mod-freq", reg->mod_freq, "Modulation frequency (Hz)")->default_val("1000");
    wave_gen_pm->add_option("--mod-index", reg->mod_index, "Modulation index")->default_val("1.0");
    wave_gen_pm->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_pm->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_pm(opts, r.rate, r.duration, r.amplitude, r.carrier_freq, r.mod_freq, r.mod_index, r.output_path); });

    auto* wave_gen_apsk16 = wave_gen->add_subcommand("apsk16", "Generate 16-APSK waveform (DVB-S2)");
    wave_gen_apsk16->add_option("--symbol-rate", reg->symbol_rate, "Symbol rate (Hz)")->required();
    wave_gen_apsk16->add_option("--sps", reg->sps, "Samples per symbol")->default_val("4");
    wave_gen_apsk16->add_option("--rrc", reg->rrc_alpha, "RRC roll-off factor")->default_val("0.35");
    wave_gen_apsk16->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_apsk16->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_apsk16->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_apsk16->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_apsk16(opts, r.symbol_rate, r.sps, r.rrc_alpha, r.duration, r.amplitude, r.output_path); });

    auto* wave_gen_apsk32 = wave_gen->add_subcommand("apsk32", "Generate 32-APSK waveform (DVB-S2)");
    wave_gen_apsk32->add_option("--symbol-rate", reg->symbol_rate, "Symbol rate (Hz)")->required();
    wave_gen_apsk32->add_option("--sps", reg->sps, "Samples per symbol")->default_val("4");
    wave_gen_apsk32->add_option("--rrc", reg->rrc_alpha, "RRC roll-off factor")->default_val("0.35");
    wave_gen_apsk32->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_apsk32->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_apsk32->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_apsk32->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_apsk32(opts, r.symbol_rate, r.sps, r.rrc_alpha, r.duration, r.amplitude, r.output_path); });

    auto* wave_gen_ofdm = wave_gen->add_subcommand("ofdm", "Generate OFDM waveform");
    wave_gen_ofdm->add_option("--rate", reg->rate, "Sample rate (Hz)")->required();
    wave_gen_ofdm->add_option("--duration", reg->duration, "Duration (seconds)")->required();
    wave_gen_ofdm->add_option("--amplitude", reg->amplitude, "Amplitude")->default_val("0.2");
    wave_gen_ofdm->add_option("--fft-size", reg->fft_size, "FFT size (power of 2)")->default_val("64");
    wave_gen_ofdm->add_option("--cp-size", reg->cp_size, "Cyclic prefix size (samples)")->default_val("16");
    wave_gen_ofdm->add_option("--active-subcarriers", reg->active_subcarriers, "Number of active subcarriers")->default_val("60");
    wave_gen_ofdm->add_option("-o,--output", reg->output_path, "Output file")->required();
    wave_gen_ofdm->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_gen_ofdm(opts, r.rate, r.duration, r.amplitude, r.fft_size, r.cp_size, r.active_subcarriers, r.output_path); });

    auto* wave_inspect = wave->add_subcommand("inspect", "Inspect a waveform file");
    wave_inspect->add_option("file", reg->file_path, "Waveform file")->required()->check(CLI::ExistingFile);
    wave_inspect->add_flag("--json", opts.json_output, "Output as JSON");
    wave_inspect->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_wave_inspect(opts, r.file_path); });

    // --- report subcommand ---
    auto* report = app->add_subcommand("report", "Report operations");

    auto* report_show = report->add_subcommand("show", "Show a run report");
    report_show->add_option("run_id", reg->run_id, "Run ID (partial match)")->required();
    report_show->add_flag("--json", opts.json_output, "Output as JSON");
    report_show->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_report_show(opts, r.run_id); });

    auto* report_export = report->add_subcommand("export", "Export per-emitter metrics");
    report_export->add_option("run_id", reg->run_id, "Run ID (partial match)")->required();
    report_export->add_option("-o,--output", reg->output_path, "Output file path (.csv or .json)")->required();
    report_export->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_metrics_export(opts, r.run_id, r.output_path); });

    auto* report_export_latest = report->add_subcommand("export-latest", "Export metrics from latest run");
    report_export_latest->add_option("-o,--output", reg->output_path, "Output file path (.csv or .json)")->required();
    report_export_latest->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_metrics_export_latest(opts, r.output_path); });

    // --- metrics subcommand ---
    auto* metrics_cmd = app->add_subcommand("metrics", "Metrics operations");

    auto* metrics_export = metrics_cmd->add_subcommand("export", "Export metrics from a run");
    metrics_export->add_flag("--latest", reg->metrics_latest, "Export from latest run")->required();
    metrics_export->add_option("-o,--output", reg->output_path, "Output file (default: stdout)");
    metrics_export->add_option("--format", reg->format, "Output format (json or csv)")
        ->default_val("json")
        ->check([](const std::string& val) -> std::string {
            if (val != "json" && val != "csv") return "Format must be 'json' or 'csv'";
            return {};
        });
    metrics_export->callback([&opts, &r = *reg]() {
        opts.last_exit_code = cmd_metrics_export_latest_ex(opts, r.output_path, r.format);
    });

    // --- dry-run subcommand ---
    auto* dryrun_cmd = app->add_subcommand("dry-run", "Validate and visualize scenario without execution");
    dryrun_cmd->add_option("file", reg->file_path, "Scenario JSON file")->required()->check(CLI::ExistingFile);
    dryrun_cmd->add_flag("--json", opts.json_output, "Output as JSON");
    dryrun_cmd->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_dryrun(opts, r.file_path); });

    // --- version subcommand ---
    auto* version_cmd = app->add_subcommand("version", "Show version");
    version_cmd->callback([&opts]() { opts.last_exit_code = cmd_version(opts); });

    // --- doctor subcommand ---
    auto* doctor_cmd = app->add_subcommand("doctor", "Run diagnostics");
    doctor_cmd->callback([&opts]() { opts.last_exit_code = cmd_doctor(opts); });

    // --- schema subcommand ---
    auto* schema_cmd = app->add_subcommand("schema", "Schema operations");
    auto* schema_print = schema_cmd->add_subcommand("print", "Print schema information");
    schema_print->add_flag("--json", reg->schema_json, "Output raw JSON schema");
    schema_print->add_flag("--markdown", reg->schema_markdown, "Output markdown table of waveform types");
    schema_print->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_schema_print(opts, r.schema_json, r.schema_markdown); });

    // --- calib subcommand ---
    auto* calib_cmd = app->add_subcommand("calib", "Calibration management");

    auto* calib_init = calib_cmd->add_subcommand("init", "Initialize a calibration file");
    calib_init->add_option("--device", reg->device_id, "Device ID")->required();
    calib_init->add_option("--channel", reg->channel, "Channel number")->required();
    calib_init->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_calib_init(opts, r.device_id, r.channel); });

    auto* calib_show = calib_cmd->add_subcommand("show", "Show calibration data");
    calib_show->add_option("--device", reg->device_id, "Device ID");
    calib_show->add_option("--channel", reg->channel, "Channel number")->default_val("0");
    calib_show->add_flag("--all", reg->calib_all_channels, "Show all channels for device");
    calib_show->add_flag("--json", opts.json_output, "Output as JSON");
    calib_show->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_calib_show(opts, r.device_id, r.channel, r.calib_all_channels); });

    auto* calib_import = calib_cmd->add_subcommand("import", "Import calibration data from file");
    calib_import->add_option("file", reg->file_path, "Calibration JSON file")->required()->check(CLI::ExistingFile);
    calib_import->add_option("--device", reg->device_id, "Device ID")->required();
    calib_import->add_option("--channel", reg->channel, "Channel number")->required();
    calib_import->callback([&opts, &r = *reg]() { opts.last_exit_code = cmd_calib_import(opts, r.file_path, r.device_id, r.channel); });

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

    return opts.last_exit_code;
}

} // namespace archerfish::cli
