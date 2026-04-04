#pragma once

#include <cstdint>
#include <string>

#include "archerfish/cli/app.hpp"

namespace archerfish::cli {

int cmd_calib_init(const CliOptions& opts, const std::string& device_id, uint32_t channel);
int cmd_calib_show(const CliOptions& opts, const std::string& device_id, uint32_t channel, bool show_all_channels);
int cmd_calib_import(const CliOptions& opts, const std::string& file_path, const std::string& device_id, uint32_t channel);

} // namespace archerfish::cli
