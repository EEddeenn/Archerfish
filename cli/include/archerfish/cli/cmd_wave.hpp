#pragma once

#include <string>
#include "archerfish/cli/app.hpp"

namespace archerfish::cli {

int cmd_wave_gen_cw(const CliOptions& opts, double rate, double duration, double amplitude, const std::string& output);
int cmd_wave_gen_chirp(const CliOptions& opts, double rate, double duration, double f0, double f1, double amplitude, const std::string& output);
int cmd_wave_gen_qpsk(const CliOptions& opts, double symbol_rate, int sps, double rrc_alpha, double duration, double amplitude, const std::string& output);
int cmd_wave_gen_pulse(const CliOptions& opts, double rate, double duration, double amplitude, double frequency, double pulse_width, double pri, const std::string& output);
int cmd_wave_gen_ask(const CliOptions& opts, double rate, double duration, double amplitude, double frequency, double symbol_rate, int num_levels, const std::string& output);
int cmd_wave_gen_fsk(const CliOptions& opts, double rate, double duration, double amplitude, double center_freq, double symbol_rate, int modulation_order, double deviation, const std::string& output);
int cmd_wave_gen_am(const CliOptions& opts, double rate, double duration, double amplitude, double carrier_freq, double mod_freq, double mod_depth, const std::string& output);
int cmd_wave_gen_fm(const CliOptions& opts, double rate, double duration, double amplitude, double carrier_freq, double mod_freq, double deviation, const std::string& output);
int cmd_wave_gen_pm(const CliOptions& opts, double rate, double duration, double amplitude, double carrier_freq, double mod_freq, double mod_index, const std::string& output);
int cmd_wave_gen_apsk16(const CliOptions& opts, double symbol_rate, int sps, double rrc_alpha, double duration, double amplitude, const std::string& output);
int cmd_wave_gen_apsk32(const CliOptions& opts, double symbol_rate, int sps, double rrc_alpha, double duration, double amplitude, const std::string& output);
int cmd_wave_gen_ofdm(const CliOptions& opts, double rate, double duration, double amplitude, int fft_size, int cp_size, int active_subcarriers, const std::string& output);
int cmd_wave_inspect(const CliOptions& opts, const std::string& file_path);

} // namespace archerfish::cli
