#pragma once

#include <string>

#include "archerfish/cli/app.hpp"

namespace archerfish::cli {

int cmd_schema_print(const CliOptions& opts, bool json, bool markdown);

} // namespace archerfish::cli
