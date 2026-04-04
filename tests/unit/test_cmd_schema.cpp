#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/cmd_schema.hpp>
#include <archerfish/cli/app.hpp>

using namespace archerfish::cli;

TEST_CASE("schema print returns exit code 0 with human-readable output", "[cli][schema]") {
    CliOptions opts;
    int rc = cmd_schema_print(opts, false, false);
    REQUIRE(rc == 0);
}

TEST_CASE("schema print --json returns exit code 0", "[cli][schema]") {
    CliOptions opts;
    int rc = cmd_schema_print(opts, true, false);
    REQUIRE(rc == 0);
}

TEST_CASE("schema print --markdown returns exit code 0", "[cli][schema]") {
    CliOptions opts;
    int rc = cmd_schema_print(opts, false, true);
    REQUIRE(rc == 0);
}

TEST_CASE("build_app has schema subcommand with print child", "[cli][schema]") {
    CliOptions opts;
    auto app = build_app(opts);
    auto* schema = app->get_subcommand("schema");
    REQUIRE(schema != nullptr);
    REQUIRE(schema->get_subcommand("print") != nullptr);
}
