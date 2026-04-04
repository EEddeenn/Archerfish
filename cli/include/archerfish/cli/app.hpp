#pragma once

#include <CLI/CLI.hpp>
#include <memory>
#include <string>

namespace archerfish::cli {

/// Exit codes per Design.md S16.3
enum class ExitCode : int {
    Success = 0,
    GenericFailure = 1,
    InputValidationFailure = 2,
    PlanningFailure = 3,
    PreparationFailure = 4,
    ExecutionFailure = 5,
    Underrun = 6,
    DeviceDisconnect = 7,
    TimeoutCancellation = 8
};

struct CliOptions {
    bool json_output{false};
    bool verbose{false};
    bool quiet{false};
    std::string device_id;
    int last_exit_code{0};
};

/// Build the CLI11 application with all subcommands.
/// Returns the app and a reference to the parsed options.
std::shared_ptr<CLI::App> build_app(CliOptions& opts);

/// Run the CLI with given argc/argv.
int run(int argc, char* argv[]);

} // namespace archerfish::cli
