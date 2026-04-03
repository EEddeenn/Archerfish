#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/cmd_devices.hpp>

using namespace archerfish::cli;

TEST_CASE("devices list without json returns success", "[cli][devices]") {
    CliOptions opts;
    opts.json_output = false;
    int rc = cmd_devices_list(opts);
    REQUIRE(rc == 0);
}

TEST_CASE("devices list with --json outputs JSON array", "[cli][devices]") {
    CliOptions opts;
    opts.json_output = true;
    int rc = cmd_devices_list(opts);
    REQUIRE(rc == 0);
}

TEST_CASE("devices info --device stub0 returns success", "[cli][devices]") {
    CliOptions opts;
    int rc = cmd_devices_info(opts, "stub0");
    REQUIRE(rc == 0);
}

TEST_CASE("devices info --device unknown returns failure", "[cli][devices]") {
    CliOptions opts;
    int rc = cmd_devices_info(opts, "nonexistent");
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
}
