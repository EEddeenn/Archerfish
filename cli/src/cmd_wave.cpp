#include "archerfish/cli/cmd_wave.hpp"
#include "archerfish/dsp/cw_source.hpp"
#include "archerfish/dsp/chirp_source.hpp"
#include "archerfish/dsp/modulator.hpp"
#include "archerfish/dsp/pulse_source.hpp"
#include "archerfish/dsp/ask_source.hpp"
#include "archerfish/dsp/fsk_source.hpp"
#include "archerfish/dsp/am_source.hpp"
#include "archerfish/dsp/fm_source.hpp"
#include "archerfish/dsp/pm_source.hpp"
#include "archerfish/dsp/waveform_metadata_io.hpp"
#include "archerfish/common/sample.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <complex>
#include <filesystem>
#include <fstream>
#include <vector>

namespace archerfish::cli {

namespace {

int render_and_write(dsp::ISource& source, double sample_rate, const std::string& output_path,
                     const std::string& waveform_type) {
    source.prepare();

    std::vector<std::complex<float>> all_samples;
    constexpr size_t block_size = 4096;
    std::vector<std::complex<float>> block(block_size);

    while (true) {
        size_t n = source.render_block(block.data(), block_size);
        if (n == 0) break;
        all_samples.insert(all_samples.end(), block.begin(), block.begin() + static_cast<ptrdiff_t>(n));
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        fmt::print(stderr, "Error: cannot open output file '{}'\n", output_path);
        return static_cast<int>(ExitCode::GenericFailure);
    }
    out.write(reinterpret_cast<const char*>(all_samples.data()),
              static_cast<std::streamsize>(all_samples.size() * sizeof(std::complex<float>)));
    out.close();

    size_t file_size = all_samples.size() * sizeof(std::complex<float>);
    auto metadata = source.report_metadata();
    dsp::write_sidecar(output_path, metadata, waveform_type, all_samples.size(), file_size);

    fmt::print("Wrote {} samples ({:.6f} s) to {}\n", all_samples.size(),
               static_cast<double>(all_samples.size()) / sample_rate, output_path);
    return 0;
}

common::SampleBuffer read_cf32_file(const std::string& path, double sample_rate) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return {};
    }
    auto file_size = in.tellg();
    if (file_size <= 0) {
        return {};
    }
    in.seekg(0);
    size_t sample_count = static_cast<size_t>(file_size) / sizeof(std::complex<float>);

    common::SampleBuffer buf;
    buf.sample_rate = sample_rate;
    buf.samples.resize(sample_count);
    in.read(reinterpret_cast<char*>(buf.samples.data()),
            static_cast<std::streamsize>(sample_count * sizeof(std::complex<float>)));
    return buf;
}

} // namespace

int cmd_wave_gen_cw(const CliOptions& opts, double rate, double duration, double amplitude, const std::string& output) {
    dsp::CwSource src;
    nlohmann::json params;
    params["sample_rate"] = rate;
    params["amplitude"] = amplitude;
    params["frequency_hz"] = 0.0;
    params["duration_sec"] = duration;
    src.configure(params);
    return render_and_write(src, rate, output, "cw");
}

int cmd_wave_gen_chirp(const CliOptions& opts, double rate, double duration, double f0, double f1, double amplitude, const std::string& output) {
    dsp::ChirpSource src;
    nlohmann::json params;
    params["sample_rate"] = rate;
    params["amplitude"] = amplitude;
    params["f0_hz"] = f0;
    params["f1_hz"] = f1;
    params["duration_sec"] = duration;
    src.configure(params);
    return render_and_write(src, rate, output, "chirp");
}

int cmd_wave_gen_qpsk(const CliOptions& opts, double symbol_rate, int sps, double rrc_alpha, double duration, double amplitude, const std::string& output) {
    dsp::ModulatorSource src;
    nlohmann::json params;
    params["modulation"] = "qpsk";
    params["symbol_rate"] = symbol_rate;
    params["samples_per_symbol"] = sps;
    params["rrc_alpha"] = rrc_alpha;
    params["amplitude"] = amplitude;
    params["sample_rate"] = symbol_rate * static_cast<double>(sps);
    params["duration_sec"] = duration;
    params["seed"] = 42;
    src.configure(params);
    return render_and_write(src, symbol_rate * static_cast<double>(sps), output, "qpsk");
}

int cmd_wave_gen_pulse(const CliOptions& opts, double rate, double duration, double amplitude, double frequency, double pulse_width, double pri, const std::string& output) {
    dsp::PulseSource src;
    nlohmann::json params;
    params["sample_rate"] = rate;
    params["amplitude"] = amplitude;
    params["frequency_hz"] = frequency;
    params["pulse_width_sec"] = pulse_width;
    params["pri_sec"] = pri;
    params["mode"] = "train";
    params["duration_sec"] = duration;
    src.configure(params);
    return render_and_write(src, rate, output, "pulse");
}

int cmd_wave_gen_ask(const CliOptions& opts, double rate, double duration, double amplitude, double frequency, double symbol_rate, int num_levels, const std::string& output) {
    dsp::AskSource src;
    nlohmann::json params;
    params["sample_rate"] = rate;
    params["amplitude"] = amplitude;
    params["frequency_hz"] = frequency;
    params["symbol_rate"] = symbol_rate;
    params["num_levels"] = num_levels;
    params["duration_sec"] = duration;
    params["seed"] = 42;
    src.configure(params);
    return render_and_write(src, rate, output, "ask");
}

int cmd_wave_gen_fsk(const CliOptions& opts, double rate, double duration, double amplitude, double center_freq, double symbol_rate, int modulation_order, double deviation, const std::string& output) {
    dsp::FskSource src;
    nlohmann::json params;
    params["sample_rate"] = rate;
    params["amplitude"] = amplitude;
    params["center_frequency_hz"] = center_freq;
    params["symbol_rate"] = symbol_rate;
    params["modulation_order"] = modulation_order;
    params["deviation_hz"] = deviation;
    params["duration_sec"] = duration;
    params["seed"] = 42;
    src.configure(params);
    return render_and_write(src, rate, output, "fsk");
}

int cmd_wave_gen_am(const CliOptions& opts, double rate, double duration, double amplitude, double carrier_freq, double mod_freq, double mod_depth, const std::string& output) {
    dsp::AmSource src;
    nlohmann::json params;
    params["sample_rate"] = rate;
    params["amplitude"] = amplitude;
    params["carrier_freq_hz"] = carrier_freq;
    params["mod_freq_hz"] = mod_freq;
    params["mod_depth"] = mod_depth;
    params["duration_sec"] = duration;
    src.configure(params);
    return render_and_write(src, rate, output, "am");
}

int cmd_wave_gen_fm(const CliOptions& opts, double rate, double duration, double amplitude, double carrier_freq, double mod_freq, double deviation, const std::string& output) {
    dsp::FmSource src;
    nlohmann::json params;
    params["sample_rate"] = rate;
    params["amplitude"] = amplitude;
    params["carrier_freq_hz"] = carrier_freq;
    params["mod_freq_hz"] = mod_freq;
    params["deviation_hz"] = deviation;
    params["duration_sec"] = duration;
    src.configure(params);
    return render_and_write(src, rate, output, "fm");
}

int cmd_wave_gen_pm(const CliOptions& opts, double rate, double duration, double amplitude, double carrier_freq, double mod_freq, double mod_index, const std::string& output) {
    dsp::PmSource src;
    nlohmann::json params;
    params["sample_rate"] = rate;
    params["amplitude"] = amplitude;
    params["carrier_freq_hz"] = carrier_freq;
    params["mod_freq_hz"] = mod_freq;
    params["mod_index"] = mod_index;
    params["duration_sec"] = duration;
    src.configure(params);
    return render_and_write(src, rate, output, "pm");
}

int cmd_wave_inspect(const CliOptions& opts, const std::string& file_path) {
    common::SampleBuffer buf = read_cf32_file(file_path, 0.0);

    if (buf.samples.empty()) {
        fmt::print(stderr, "Error: cannot read file or file is empty: {}\n", file_path);
        return static_cast<int>(ExitCode::GenericFailure);
    }

    auto sidecar = dsp::read_sidecar(file_path);

    if (opts.json_output) {
        nlohmann::json out = {
            {"format", "CF32"},
            {"samples", buf.count()},
            {"bytes", buf.count() * sizeof(std::complex<float>)},
            {"peak_amplitude", buf.peak_amplitude()},
            {"rms_amplitude", buf.rms_amplitude()},
            {"crest_factor", buf.crest_factor()},
        };
        if (sidecar) {
            out["sidecar"] = nlohmann::json::parse(dsp::format_sidecar_json(*sidecar));
        }
        fmt::print("{}\n", out.dump(2));
    } else {
        fmt::print("Format:         CF32 (complex<float32>)\n");
        fmt::print("Samples:        {}\n", buf.count());
        fmt::print("File size:      {} bytes\n", buf.count() * sizeof(std::complex<float>));
        fmt::print("Peak amplitude: {:.6f}\n", buf.peak_amplitude());
        fmt::print("RMS amplitude:  {:.6f}\n", buf.rms_amplitude());
        fmt::print("Crest factor:   {:.6f}\n", buf.crest_factor());

        if (sidecar) {
            fmt::print("\n--- Sidecar Metadata ---\n{}\n", dsp::format_sidecar_human(*sidecar));
        }
    }

    return 0;
}

} // namespace archerfish::cli
