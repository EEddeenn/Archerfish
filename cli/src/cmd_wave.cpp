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
#include "archerfish/dsp/ofdm_source.hpp"
#include "archerfish/dsp/waveform_metadata_io.hpp"
#include "archerfish/common/sample.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>
#include <vector>

namespace archerfish::cli {

namespace {

constexpr size_t kMaxInspectSamples = 16'000'000;

int invalid_argument(std::string_view message) {
    fmt::print(stderr, "Error: {}\n", message);
    return static_cast<int>(ExitCode::InputValidationFailure);
}

bool is_valid_number(double value) {
    return std::isfinite(value);
}

bool validate_finite(std::string_view name, double value) {
    if (!is_valid_number(value)) {
        fmt::print(stderr, "Error: {} must be finite\n", name);
        return false;
    }
    return true;
}

bool validate_positive(std::string_view name, double value) {
    if (!validate_finite(name, value)) return false;
    if (value <= 0.0) {
        fmt::print(stderr, "Error: {} must be positive\n", name);
        return false;
    }
    return true;
}

bool validate_non_negative(std::string_view name, double value) {
    if (!validate_finite(name, value)) return false;
    if (value < 0.0) {
        fmt::print(stderr, "Error: {} must be non-negative\n", name);
        return false;
    }
    return true;
}

bool validate_common_wave_args(double sample_rate, double duration, double amplitude) {
    return validate_positive("sample rate", sample_rate) &&
           validate_positive("duration", duration) &&
           validate_non_negative("amplitude", amplitude);
}

bool validate_positive_int(std::string_view name, int value) {
    if (value <= 0) {
        fmt::print(stderr, "Error: {} must be positive\n", name);
        return false;
    }
    return true;
}

bool validate_modulation_args(double symbol_rate, int sps, double rrc_alpha,
                              double duration, double amplitude) {
    if (!validate_positive("symbol rate", symbol_rate) ||
        !validate_positive_int("samples per symbol", sps) ||
        !validate_finite("RRC alpha", rrc_alpha) ||
        !validate_common_wave_args(symbol_rate * static_cast<double>(sps), duration, amplitude)) {
        return false;
    }
    if (rrc_alpha <= 0.0 || rrc_alpha > 1.0) {
        fmt::print(stderr, "Error: RRC alpha must be in (0, 1]\n");
        return false;
    }
    return true;
}

template <typename Callable>
int run_wave_command(Callable&& callable) {
    try {
        return callable();
    } catch (const std::bad_alloc&) {
        fmt::print(stderr, "Error: waveform generation ran out of memory\n");
        return static_cast<int>(ExitCode::GenericFailure);
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }
}

int render_and_write(dsp::ISource& source, double sample_rate, const std::string& output_path,
                     const std::string& waveform_type) {
    if (output_path.empty()) {
        return invalid_argument("output path must not be empty");
    }

    source.prepare();

    constexpr size_t block_size = 4096;
    std::vector<std::complex<float>> block(block_size);

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        fmt::print(stderr, "Error: cannot open output file '{}'\n", output_path);
        return static_cast<int>(ExitCode::GenericFailure);
    }

    size_t sample_count = 0;
    while (true) {
        size_t n = source.render_block(block.data(), block_size);
        if (n == 0) break;
        out.write(reinterpret_cast<const char*>(block.data()),
                  static_cast<std::streamsize>(n * sizeof(std::complex<float>)));
        if (!out) {
            fmt::print(stderr, "Error: failed while writing '{}'\n", output_path);
            return static_cast<int>(ExitCode::GenericFailure);
        }
        sample_count += n;
    }
    out.close();

    size_t file_size = sample_count * sizeof(std::complex<float>);
    auto metadata = source.report_metadata();
    if (!dsp::write_sidecar(output_path, metadata, waveform_type, sample_count, file_size)) {
        fmt::print(stderr, "Error: failed to write sidecar metadata for '{}'\n", output_path);
        return static_cast<int>(ExitCode::GenericFailure);
    }

    fmt::print("Wrote {} samples ({:.6f} s) to {}\n", sample_count,
               static_cast<double>(sample_count) / sample_rate, output_path);
    return 0;
}

common::SampleBuffer read_cf32_file(const std::string& path, double sample_rate) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return {};
    }
    auto end_pos = in.tellg();
    if (end_pos <= 0) {
        return {};
    }
    const auto file_size = static_cast<std::streamoff>(end_pos);
    if (file_size > std::numeric_limits<std::streamsize>::max()) {
        return {};
    }
    if (static_cast<std::uintmax_t>(file_size) % sizeof(std::complex<float>) != 0) {
        return {};
    }
    in.seekg(0);
    size_t sample_count = static_cast<size_t>(file_size) / sizeof(std::complex<float>);
    if (sample_count > kMaxInspectSamples ||
        sample_count > static_cast<size_t>(std::numeric_limits<std::streamsize>::max()) /
                           sizeof(std::complex<float>)) {
        return {};
    }

    common::SampleBuffer buf;
    buf.sample_rate = sample_rate;
    buf.samples.resize(sample_count);
    in.read(reinterpret_cast<char*>(buf.samples.data()),
            static_cast<std::streamsize>(sample_count * sizeof(std::complex<float>)));
    if (!in) {
        return {};
    }
    return buf;
}

} // namespace

int cmd_wave_gen_cw(const CliOptions& opts, double rate, double duration, double amplitude, const std::string& output) {
    (void)opts;
    if (!validate_common_wave_args(rate, duration, amplitude)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    return run_wave_command([&]() {
        dsp::CwSource src;
        nlohmann::json params;
        params["sample_rate"] = rate;
        params["amplitude"] = amplitude;
        params["frequency_hz"] = 0.0;
        params["duration_sec"] = duration;
        src.configure(params);
        return render_and_write(src, rate, output, "cw");
    });
}

int cmd_wave_gen_chirp(const CliOptions& opts, double rate, double duration, double f0, double f1, double amplitude, const std::string& output) {
    (void)opts;
    if (!validate_common_wave_args(rate, duration, amplitude) ||
        !validate_finite("start frequency", f0) ||
        !validate_finite("end frequency", f1)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    return run_wave_command([&]() {
        dsp::ChirpSource src;
        nlohmann::json params;
        params["sample_rate"] = rate;
        params["amplitude"] = amplitude;
        params["f0_hz"] = f0;
        params["f1_hz"] = f1;
        params["duration_sec"] = duration;
        src.configure(params);
        return render_and_write(src, rate, output, "chirp");
    });
}

int cmd_wave_gen_qpsk(const CliOptions& opts, double symbol_rate, int sps, double rrc_alpha, double duration, double amplitude, const std::string& output) {
    (void)opts;
    if (!validate_modulation_args(symbol_rate, sps, rrc_alpha, duration, amplitude)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    return run_wave_command([&]() {
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
    });
}

int cmd_wave_gen_pulse(const CliOptions& opts, double rate, double duration, double amplitude, double frequency, double pulse_width, double pri, const std::string& output) {
    (void)opts;
    if (!validate_common_wave_args(rate, duration, amplitude) ||
        !validate_finite("frequency", frequency) ||
        !validate_positive("pulse width", pulse_width) ||
        !validate_positive("PRI", pri)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    if (pulse_width > pri) {
        return invalid_argument("pulse width must not exceed PRI");
    }
    return run_wave_command([&]() {
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
    });
}

int cmd_wave_gen_ask(const CliOptions& opts, double rate, double duration, double amplitude, double frequency, double symbol_rate, int num_levels, const std::string& output) {
    (void)opts;
    if (!validate_common_wave_args(rate, duration, amplitude) ||
        !validate_finite("frequency", frequency) ||
        !validate_positive("symbol rate", symbol_rate) ||
        !validate_positive_int("number of levels", num_levels)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    if (num_levels < 2) {
        return invalid_argument("number of levels must be at least 2");
    }
    return run_wave_command([&]() {
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
    });
}

int cmd_wave_gen_fsk(const CliOptions& opts, double rate, double duration, double amplitude, double center_freq, double symbol_rate, int modulation_order, double deviation, const std::string& output) {
    (void)opts;
    if (!validate_common_wave_args(rate, duration, amplitude) ||
        !validate_finite("center frequency", center_freq) ||
        !validate_positive("symbol rate", symbol_rate) ||
        !validate_positive_int("modulation order", modulation_order) ||
        !validate_non_negative("deviation", deviation)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    if (modulation_order < 2) {
        return invalid_argument("modulation order must be at least 2");
    }
    return run_wave_command([&]() {
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
    });
}

int cmd_wave_gen_am(const CliOptions& opts, double rate, double duration, double amplitude, double carrier_freq, double mod_freq, double mod_depth, const std::string& output) {
    (void)opts;
    if (!validate_common_wave_args(rate, duration, amplitude) ||
        !validate_finite("carrier frequency", carrier_freq) ||
        !validate_positive("modulation frequency", mod_freq) ||
        !validate_non_negative("modulation depth", mod_depth)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    return run_wave_command([&]() {
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
    });
}

int cmd_wave_gen_fm(const CliOptions& opts, double rate, double duration, double amplitude, double carrier_freq, double mod_freq, double deviation, const std::string& output) {
    (void)opts;
    if (!validate_common_wave_args(rate, duration, amplitude) ||
        !validate_finite("carrier frequency", carrier_freq) ||
        !validate_positive("modulation frequency", mod_freq) ||
        !validate_non_negative("deviation", deviation)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    return run_wave_command([&]() {
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
    });
}

int cmd_wave_gen_pm(const CliOptions& opts, double rate, double duration, double amplitude, double carrier_freq, double mod_freq, double mod_index, const std::string& output) {
    (void)opts;
    if (!validate_common_wave_args(rate, duration, amplitude) ||
        !validate_finite("carrier frequency", carrier_freq) ||
        !validate_positive("modulation frequency", mod_freq) ||
        !validate_non_negative("modulation index", mod_index)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    return run_wave_command([&]() {
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
    });
}

int cmd_wave_gen_apsk16(const CliOptions& opts, double symbol_rate, int sps, double rrc_alpha, double duration, double amplitude, const std::string& output) {
    (void)opts;
    if (!validate_modulation_args(symbol_rate, sps, rrc_alpha, duration, amplitude)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    return run_wave_command([&]() {
        dsp::ModulatorSource src;
        nlohmann::json params;
        params["modulation"] = "apsk16";
        params["symbol_rate"] = symbol_rate;
        params["samples_per_symbol"] = sps;
        params["rrc_alpha"] = rrc_alpha;
        params["amplitude"] = amplitude;
        params["sample_rate"] = symbol_rate * static_cast<double>(sps);
        params["duration_sec"] = duration;
        params["seed"] = 42;
        src.configure(params);
        return render_and_write(src, symbol_rate * static_cast<double>(sps), output, "apsk16");
    });
}

int cmd_wave_gen_apsk32(const CliOptions& opts, double symbol_rate, int sps, double rrc_alpha, double duration, double amplitude, const std::string& output) {
    (void)opts;
    if (!validate_modulation_args(symbol_rate, sps, rrc_alpha, duration, amplitude)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    return run_wave_command([&]() {
        dsp::ModulatorSource src;
        nlohmann::json params;
        params["modulation"] = "apsk32";
        params["symbol_rate"] = symbol_rate;
        params["samples_per_symbol"] = sps;
        params["rrc_alpha"] = rrc_alpha;
        params["amplitude"] = amplitude;
        params["sample_rate"] = symbol_rate * static_cast<double>(sps);
        params["duration_sec"] = duration;
        params["seed"] = 42;
        src.configure(params);
        return render_and_write(src, symbol_rate * static_cast<double>(sps), output, "apsk32");
    });
}

int cmd_wave_gen_ofdm(const CliOptions& opts, double rate, double duration, double amplitude, int fft_size, int cp_size, int active_subcarriers, const std::string& output) {
    (void)opts;
    if (!validate_common_wave_args(rate, duration, amplitude) ||
        !validate_positive_int("FFT size", fft_size) ||
        !validate_positive_int("cyclic prefix size", cp_size) ||
        !validate_positive_int("active subcarriers", active_subcarriers)) {
        return static_cast<int>(ExitCode::InputValidationFailure);
    }
    const bool power_of_two = (fft_size & (fft_size - 1)) == 0;
    if (!power_of_two) {
        return invalid_argument("FFT size must be a power of two");
    }
    if (cp_size >= fft_size) {
        return invalid_argument("cyclic prefix size must be smaller than FFT size");
    }
    if (active_subcarriers >= fft_size) {
        return invalid_argument("active subcarriers must be smaller than FFT size");
    }
    return run_wave_command([&]() {
        dsp::OfdmSource src;
        nlohmann::json params;
        params["sample_rate"] = rate;
        params["amplitude"] = amplitude;
        params["fft_size"] = fft_size;
        params["cyclic_prefix_size"] = cp_size;
        params["active_subcarriers"] = active_subcarriers;
        params["duration_sec"] = duration;
        params["seed"] = 42;
        src.configure(params);
        return render_and_write(src, rate, output, "ofdm");
    });
}

int cmd_wave_inspect(const CliOptions& opts, const std::string& file_path) {
    return run_wave_command([&]() {
        common::SampleBuffer buf = read_cf32_file(file_path, 0.0);

        if (buf.samples.empty()) {
            fmt::print(stderr, "Error: cannot read file or file is empty: {}\n", file_path);
            return static_cast<int>(ExitCode::GenericFailure);
        }

        auto sidecar = dsp::read_sidecar(file_path);
        const size_t inspected_bytes = buf.count() * sizeof(std::complex<float>);
        if (sidecar && (sidecar->num_samples != buf.count() ||
                        sidecar->file_size_bytes != inspected_bytes)) {
            sidecar.reset();
        }

        if (opts.json_output) {
            nlohmann::json out = {
                {"format", "CF32"},
                {"samples", buf.count()},
                {"bytes", inspected_bytes},
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
            fmt::print("File size:      {} bytes\n", inspected_bytes);
            fmt::print("Peak amplitude: {:.6f}\n", buf.peak_amplitude());
            fmt::print("RMS amplitude:  {:.6f}\n", buf.rms_amplitude());
            fmt::print("Crest factor:   {:.6f}\n", buf.crest_factor());

            if (sidecar) {
                fmt::print("\n--- Sidecar Metadata ---\n{}\n", dsp::format_sidecar_human(*sidecar));
            }
        }

        return 0;
    });
}

} // namespace archerfish::cli
