#pragma once

#include <string>

#include "archerfish/cli/app.hpp"

namespace archerfish::cli {

int cmd_report_show(const CliOptions& opts, const std::string& run_id);
int cmd_metrics_export(const CliOptions& opts, const std::string& run_id, const std::string& output_path);
int cmd_metrics_export_latest(const CliOptions& opts, const std::string& output_path);

int cmd_metrics_export_latest_ex(const CliOptions& opts,
                                 const std::string& output_path,
                                 const std::string& format);

} // namespace archerfish::cli
