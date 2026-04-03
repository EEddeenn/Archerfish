#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/cmd_scenario.hpp>

using namespace archerfish::cli;

static const char* examples_dir = EXAMPLES_DIR;

TEST_CASE("scenario validate on valid file returns exit code 0", "[cli][scenario]") {
    CliOptions opts;
    std::string path = std::string(examples_dir) + "/future_start_cw.json";
    int rc = cmd_scenario_validate(opts, path);
    REQUIRE(rc == 0);
}

TEST_CASE("scenario validate on invalid file returns exit code 2", "[cli][scenario]") {
    CliOptions opts;
    int rc = cmd_scenario_validate(opts, "/nonexistent/file.json");
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
}

TEST_CASE("scenario plan on valid file returns exit code 0", "[cli][scenario]") {
    CliOptions opts;
    std::string path = std::string(examples_dir) + "/future_start_cw.json";
    int rc = cmd_scenario_plan(opts, path);
    REQUIRE(rc == 0);
}

TEST_CASE("scenario plan --json on valid file returns exit code 0", "[cli][scenario]") {
    CliOptions opts;
    opts.json_output = true;
    std::string path = std::string(examples_dir) + "/future_start_cw.json";
    int rc = cmd_scenario_plan(opts, path);
    REQUIRE(rc == 0);
}

TEST_CASE("scenario run on valid file returns exit code 0", "[cli][scenario]") {
    CliOptions opts;
    std::string path = std::string(examples_dir) + "/future_start_cw.json";
    int rc = cmd_scenario_run(opts, path);
    REQUIRE(rc == 0);
}
