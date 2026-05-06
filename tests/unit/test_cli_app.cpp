#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/app.hpp>
#include <CLI/CLI.hpp>

#include <vector>

using namespace archerfish::cli;

TEST_CASE("build_app returns valid CLI11 app", "[cli]") {
    CliOptions opts;
    auto app = build_app(opts);
    REQUIRE(app != nullptr);
}

TEST_CASE("build_app has required subcommands", "[cli]") {
    CliOptions opts;
    auto app = build_app(opts);

    auto* sub = app->get_subcommand("devices");
    REQUIRE(sub != nullptr);
    REQUIRE(sub->get_subcommand("list") != nullptr);
    REQUIRE(sub->get_subcommand("info") != nullptr);

    auto* scenario = app->get_subcommand("scenario");
    REQUIRE(scenario != nullptr);
    REQUIRE(scenario->get_subcommand("validate") != nullptr);
    REQUIRE(scenario->get_subcommand("plan") != nullptr);
    REQUIRE(scenario->get_subcommand("run") != nullptr);

    auto* wave = app->get_subcommand("wave");
    REQUIRE(wave != nullptr);
    auto* gen = wave->get_subcommand("gen");
    REQUIRE(gen != nullptr);
    REQUIRE(gen->get_subcommand("cw") != nullptr);
    REQUIRE(gen->get_subcommand("chirp") != nullptr);
    REQUIRE(gen->get_subcommand("qpsk") != nullptr);
    REQUIRE(wave->get_subcommand("inspect") != nullptr);

    REQUIRE(app->get_subcommand("version") != nullptr);
    REQUIRE(app->get_subcommand("doctor") != nullptr);
}

TEST_CASE("version flag works", "[cli]") {
    CliOptions opts;
    auto app = build_app(opts);

    const char* argv[] = {"archerfish", "--version"};
    REQUIRE_THROWS_AS(app->parse(2, const_cast<char**>(argv)), CLI::CallForVersion);
}

TEST_CASE("command groups require an action subcommand", "[cli]") {
    auto run_args = [](std::initializer_list<const char*> args) {
        std::vector<char*> argv;
        argv.reserve(args.size());
        for (const char* arg : args) {
            argv.push_back(const_cast<char*>(arg));
        }
        return run(static_cast<int>(argv.size()), argv.data());
    };

    CHECK(run_args({"archerfish", "devices"}) != 0);
    CHECK(run_args({"archerfish", "scenario"}) != 0);
    CHECK(run_args({"archerfish", "wave"}) != 0);
    CHECK(run_args({"archerfish", "wave", "gen"}) != 0);
    CHECK(run_args({"archerfish", "report"}) != 0);
    CHECK(run_args({"archerfish", "metrics"}) != 0);
    CHECK(run_args({"archerfish", "schema"}) != 0);
    CHECK(run_args({"archerfish", "calib"}) != 0);
}
