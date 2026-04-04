#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/app.hpp>
#include <archerfish/cli/cmd_report.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using namespace archerfish::cli;

namespace {

struct TempDirGuard {
    fs::path original;
    fs::path temp_dir;

    TempDirGuard() {
        original = fs::current_path();
        temp_dir = fs::temp_directory_path() / ("archerfish_test_metrics_" + std::to_string(::getpid()));
        fs::create_directories(temp_dir);
        fs::current_path(temp_dir);
    }

    ~TempDirGuard() {
        fs::current_path(original);
        fs::remove_all(temp_dir);
    }
};

void create_run(const std::string& name, const nlohmann::json& metrics) {
    fs::create_directories("runs/" + name);
    std::ofstream f("runs/" + name + "/metrics.json");
    f << metrics.dump(2);
}

nlohmann::json sample_metrics() {
    return {
        {"start_requested_sec", 0.0},
        {"start_actual_sec", 1.5},
        {"stop_actual_sec", 2.5},
        {"tx_duration_sec", 1.0},
        {"underrun_count", 0},
        {"late_command_count", 0},
        {"queue_depth_stats", {{"avg_depth", 5.0}, {"max_depth", 10.0}, {"overflow_count", 0}}},
        {"warning_count", 0},
        {"error_count", 0},
        {"total_samples_sent", 10000000},
        {"scenario_hash", "abc123"},
    };
}

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("Export returns error when no runs directory exists", "[metrics_export]") {
    TempDirGuard guard;
    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "", "json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Export returns error when runs directory is empty", "[metrics_export]") {
    TempDirGuard guard;
    fs::create_directories("runs");
    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "", "json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Export succeeds when valid metrics.json exists", "[metrics_export]") {
    TempDirGuard guard;
    create_run("2026-01-01T120000Z_test", sample_metrics());

    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "out.json", "json");
    REQUIRE(rc == 0);

    nlohmann::json result = nlohmann::json::parse(read_file("out.json"));
    REQUIRE(result["start_actual_sec"] == 1.5);
    REQUIRE(result["scenario_hash"] == "abc123");
    REQUIRE(result["queue_depth_stats"]["avg_depth"] == 5.0);
}

TEST_CASE("CSV export formats correctly", "[metrics_export]") {
    TempDirGuard guard;
    create_run("2026-01-01T120000Z_test", sample_metrics());

    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "out.csv", "csv");
    REQUIRE(rc == 0);

    std::string csv = read_file("out.csv");
    REQUIRE(csv.find("key,value\n") == 0);
    REQUIRE(csv.find("start_actual_sec,1.5\n") != std::string::npos);
    REQUIRE(csv.find("queue_depth_stats.avg_depth,5.0\n") != std::string::npos);
    REQUIRE(csv.find("scenario_hash,abc123\n") != std::string::npos);
}

TEST_CASE("Export to stdout returns success", "[metrics_export]") {
    TempDirGuard guard;
    create_run("2026-01-01T120000Z_test", sample_metrics());

    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "", "json");
    REQUIRE(rc == 0);
}

TEST_CASE("CLI app builds with metrics export subcommand", "[metrics_export][cli]") {
    CliOptions opts;
    auto app = build_app(opts);
    REQUIRE(app != nullptr);

    auto* metrics = app->get_subcommand("metrics");
    REQUIRE(metrics != nullptr);

    auto* metrics_export = metrics->get_subcommand("export");
    REQUIRE(metrics_export != nullptr);
}
