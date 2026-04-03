#pragma once

#include <string>

#include "archerfish/cli/app.hpp"

namespace archerfish::cli {

int cmd_report_show(const CliOptions& opts, const std::string& run_id);
int cmd_metrics_export(const CliOptions& opts, const std::string& run_id, const std::string& output_path);

} // namespace archerfish::cli
