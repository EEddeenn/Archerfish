#pragma once

#include <string>

#include "archerfish/cli/app.hpp"

namespace archerfish::cli {

int cmd_dryrun(const CliOptions& opts, const std::string& file_path);

} // namespace archerfish::cli
