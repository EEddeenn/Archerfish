#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/app.hpp>
#include <CLI/CLI.hpp>

using namespace archerfish::cli;

TEST_CASE("CLI has scenario subcommand with validate/plan/run", "[cli][subcommands]") {
    CliOptions opts;
    auto app = build_app(opts);
    auto* scenario = app->get_subcommand("scenario");
    REQUIRE(scenario != nullptr);
    REQUIRE(scenario->get_subcommand("validate") != nullptr);
    REQUIRE(scenario->get_subcommand("plan") != nullptr);
    REQUIRE(scenario->get_subcommand("run") != nullptr);
}

TEST_CASE("CLI has wave subcommand with gen and inspect", "[cli][subcommands]") {
    CliOptions opts;
    auto app = build_app(opts);
    auto* wave = app->get_subcommand("wave");
    REQUIRE(wave != nullptr);
    REQUIRE(wave->get_subcommand("gen") != nullptr);
    REQUIRE(wave->get_subcommand("inspect") != nullptr);
}

TEST_CASE("CLI wave gen has cw, chirp, qpsk, pulse subcommands", "[cli][subcommands]") {
    CliOptions opts;
    auto app = build_app(opts);
    auto* gen = app->get_subcommand("wave")->get_subcommand("gen");
    REQUIRE(gen->get_subcommand("cw") != nullptr);
    REQUIRE(gen->get_subcommand("chirp") != nullptr);
    REQUIRE(gen->get_subcommand("qpsk") != nullptr);
    REQUIRE(gen->get_subcommand("pulse") != nullptr);
}

TEST_CASE("CLI has doctor and version subcommands", "[cli][subcommands]") {
    CliOptions opts;
    auto app = build_app(opts);
    REQUIRE(app->get_subcommand("version") != nullptr);
    REQUIRE(app->get_subcommand("doctor") != nullptr);
}

TEST_CASE("CLI has devices subcommand with list and info", "[cli][subcommands]") {
    CliOptions opts;
    auto app = build_app(opts);
    auto* devices = app->get_subcommand("devices");
    REQUIRE(devices != nullptr);
    REQUIRE(devices->get_subcommand("list") != nullptr);
    REQUIRE(devices->get_subcommand("info") != nullptr);
}

TEST_CASE("CLI has schema and calib subcommands", "[cli][subcommands]") {
    CliOptions opts;
    auto app = build_app(opts);
    REQUIRE(app->get_subcommand("schema") != nullptr);
    REQUIRE(app->get_subcommand("calib") != nullptr);
}
