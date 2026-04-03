#pragma once

#include <string>
#include <archerfish/cli/app.hpp>

namespace archerfish::cli {

int cmd_scenario_validate(const CliOptions& opts, const std::string& file_path);
int cmd_scenario_plan(const CliOptions& opts, const std::string& file_path);
int cmd_scenario_run(const CliOptions& opts, const std::string& file_path);

} // namespace archerfish::cli
