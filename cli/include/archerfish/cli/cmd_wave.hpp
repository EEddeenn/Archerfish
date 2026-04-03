#pragma once

#include <string>
#include <archerfish/cli/app.hpp>

namespace archerfish::cli {

int cmd_wave_gen_cw(const CliOptions& opts, double rate, double duration, double amplitude, const std::string& output);
int cmd_wave_gen_chirp(const CliOptions& opts, double rate, double duration, double f0, double f1, double amplitude, const std::string& output);
int cmd_wave_gen_qpsk(const CliOptions& opts, double symbol_rate, int sps, double rrc_alpha, double duration, double amplitude, const std::string& output);
int cmd_wave_inspect(const CliOptions& opts, const std::string& file_path);

} // namespace archerfish::cli
