#pragma once

#include <string>
#include <archerfish/cli/app.hpp>

namespace archerfish::cli {

int cmd_devices_list(const CliOptions& opts);
int cmd_devices_info(const CliOptions& opts, const std::string& device_id);

} // namespace archerfish::cli
