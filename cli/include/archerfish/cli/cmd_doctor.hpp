#pragma once

#include <string>
#include "archerfish/cli/app.hpp"

namespace archerfish::cli {

int cmd_version(const CliOptions& opts);
int cmd_doctor(const CliOptions& opts);

} // namespace archerfish::cli
