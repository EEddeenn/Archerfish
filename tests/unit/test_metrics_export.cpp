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

void create_run_metrics_text(const std::string& name, const std::string& metrics_text) {
    fs::create_directories("runs/" + name);
    std::ofstream f("runs/" + name + "/metrics.json");
    f << metrics_text;
}

void create_run_report_text(const std::string& name, const std::string& report_text) {
    fs::create_directories("runs/" + name);
    std::ofstream f("runs/" + name + "/report.json");
    f << report_text;
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

nlohmann::json sample_report() {
    return {
        {"scenario_name", "test_scenario"},
        {"scenario_hash", "abc123"},
        {"status", "completed"},
        {"planned_start_sec", 0.0},
        {"actual_start_sec", 0.0},
        {"actual_stop_sec", 1.0},
        {"actual_duration_sec", 1.0},
        {"devices", nlohmann::json::array()},
        {"warnings", nlohmann::json::array()},
        {"errors", nlohmann::json::array()},
        {"artifact_paths", nlohmann::json::array()},
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

TEST_CASE("Latest metrics export uses newest run id, not newest directory mtime", "[metrics_export]") {
    TempDirGuard guard;

    auto latest_metrics = sample_metrics();
    latest_metrics["scenario_hash"] = "latest";
    latest_metrics["start_actual_sec"] = 12.0;
    latest_metrics["stop_actual_sec"] = 13.0;
    latest_metrics["tx_duration_sec"] = 1.0;
    create_run("2026-01-01T120000Z_test", latest_metrics);

    auto old_metrics = sample_metrics();
    old_metrics["scenario_hash"] = "old";
    old_metrics["start_actual_sec"] = 10.0;
    old_metrics["stop_actual_sec"] = 11.0;
    old_metrics["tx_duration_sec"] = 1.0;
    create_run("2026-01-01T100000Z_test", old_metrics);

    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "out.json", "json");
    REQUIRE(rc == 0);

    nlohmann::json result = nlohmann::json::parse(read_file("out.json"));
    REQUIRE(result["scenario_hash"] == "latest");
    REQUIRE(result["start_actual_sec"] == 12.0);
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

TEST_CASE("Export returns error when latest metrics JSON is malformed", "[metrics_export]") {
    TempDirGuard guard;
    create_run_metrics_text("2026-01-01T120000Z_test", R"({"start_actual_sec":)");

    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "", "json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Export returns error when latest metrics JSON has invalid field values", "[metrics_export]") {
    TempDirGuard guard;
    auto metrics = sample_metrics();
    metrics["underrun_count"] = -1;
    create_run("2026-01-01T120000Z_test", metrics);

    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "", "json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));

    rc = cmd_metrics_export_latest_ex(opts, "out.csv", "csv");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
    REQUIRE_FALSE(fs::exists("out.csv"));
}

TEST_CASE("Export returns error when latest metrics JSON has invalid containers", "[metrics_export]") {
    TempDirGuard guard;
    auto metrics = sample_metrics();
    metrics["queue_depth_stats"] = "none";
    create_run("2026-01-01T120000Z_test", metrics);

    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "", "json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Export returns error when output path is a directory", "[metrics_export]") {
    TempDirGuard guard;
    create_run("2026-01-01T120000Z_test", sample_metrics());
    fs::create_directories("outdir");

    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "outdir", "json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));

    rc = cmd_metrics_export_latest_ex(opts, "outdir", "csv");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Export returns error for unsupported format", "[metrics_export]") {
    TempDirGuard guard;
    create_run("2026-01-01T120000Z_test", sample_metrics());

    CliOptions opts;
    int rc = cmd_metrics_export_latest_ex(opts, "", "xml");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report show returns error when report JSON is malformed", "[metrics_export][report]") {
    TempDirGuard guard;
    create_run_report_text("2026-01-01T120000Z_test", R"({"scenario_name":)");

    CliOptions opts;
    int rc = cmd_report_show(opts, "2026-01-01");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report metrics export returns error when report JSON is malformed", "[metrics_export][report]") {
    TempDirGuard guard;
    create_run_report_text("2026-01-01T120000Z_test", R"({"scenario_name":)");

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report commands reject blank run ids before matching runs", "[metrics_export][report]") {
    TempDirGuard guard;
    create_run_report_text("2026-01-01T120000Z_test", sample_report().dump());

    CliOptions opts;
    CHECK(cmd_report_show(opts, "") == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK(cmd_report_show(opts, " \t\n") == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK(cmd_metrics_export(opts, "", "out.json") == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK(cmd_metrics_export(opts, " \t\n", "out.json") ==
          static_cast<int>(ExitCode::InputValidationFailure));
    CHECK_FALSE(fs::exists("out.json"));
}

TEST_CASE("Report metrics export rejects empty output paths", "[metrics_export][report]") {
    TempDirGuard guard;
    create_run_report_text("2026-01-01T120000Z_test", sample_report().dump());

    CliOptions opts;
    CHECK(cmd_metrics_export(opts, "2026-01-01", "") ==
          static_cast<int>(ExitCode::InputValidationFailure));
}

TEST_CASE("Report show returns error when report JSON has invalid field values", "[metrics_export][report]") {
    TempDirGuard guard;
    create_run_report_text("2026-01-01T120000Z_test", R"({"actual_duration_sec": -1})");

    CliOptions opts;
    int rc = cmd_report_show(opts, "2026-01-01");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report show --json validates report JSON before printing", "[metrics_export][report]") {
    TempDirGuard guard;
    create_run_report_text("2026-01-01T120000Z_test", R"({"actual_duration_sec": -1})");

    CliOptions opts;
    opts.json_output = true;
    int rc = cmd_report_show(opts, "2026-01-01");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report metrics export returns error when emitter metrics have wrong types", "[metrics_export][report]") {
    TempDirGuard guard;
    create_run_report_text("2026-01-01T120000Z_test",
                           R"({"emitters": [{"emitter_id": "e1", "samples_rendered": "many"}]})");

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report metrics export rejects negative emitter metric values", "[metrics_export][report]") {
    TempDirGuard guard;
    create_run_report_text("2026-01-01T120000Z_test",
                           R"({"emitters": [{"emitter_id": "e1", "samples_rendered": -1}]})");

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report metrics export rejects malformed emitter string and boolean fields", "[metrics_export][report]") {
    TempDirGuard guard;
    auto report = sample_report();
    report["emitters"] = nlohmann::json::array({
        {
            {"emitter_id", "e1"},
            {"device_id", "dev0"},
            {"waveform_type", 42},
            {"completed", true},
        },
    });
    create_run_report_text("2026-01-01T120000Z_test", report.dump());

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));

    report["emitters"][0]["waveform_type"] = "cw";
    report["emitters"][0]["completed"] = "true";
    create_run_report_text("2026-01-01T120000Z_test", report.dump());

    rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report metrics export requires nonblank emitter ids", "[metrics_export][report]") {
    TempDirGuard guard;
    auto report = sample_report();
    report["emitters"] = nlohmann::json::array({
        {
            {"device_id", "dev0"},
            {"completed", true},
        },
    });
    create_run_report_text("2026-01-01T120000Z_test", report.dump());

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));

    report["emitters"][0]["emitter_id"] = " \t\n";
    create_run_report_text("2026-01-01T120000Z_test", report.dump());

    rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report metrics export rejects malformed emitter containers", "[metrics_export][report]") {
    TempDirGuard guard;
    auto report = sample_report();
    report["emitters"] = {{"e1", nlohmann::json::object()}};
    create_run_report_text("2026-01-01T120000Z_test", report.dump());

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));

    report["emitters"] = nlohmann::json::array({"e1"});
    create_run_report_text("2026-01-01T120000Z_test", report.dump());

    rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report metrics export rejects duplicate emitter ids", "[metrics_export][report]") {
    TempDirGuard guard;
    auto report = sample_report();
    report["emitters"] = nlohmann::json::array({
        {
            {"emitter_id", "e1"},
            {"device_id", "dev0"},
            {"start_time_sec", 0.0},
            {"duration_sec", 1.0},
            {"samples_rendered", 128},
            {"peak_amplitude", 0.75},
            {"rms_amplitude", 0.25},
            {"crest_factor", 3.0},
            {"nominal_bandwidth", 1000.0},
            {"waveform_type", "cw"},
            {"completed", true},
        },
        {
            {"emitter_id", "e1"},
            {"device_id", "dev0"},
            {"start_time_sec", 1.0},
            {"duration_sec", 1.0},
            {"samples_rendered", 256},
            {"peak_amplitude", 0.5},
            {"rms_amplitude", 0.2},
            {"crest_factor", 2.5},
            {"nominal_bandwidth", 1000.0},
            {"waveform_type", "cw"},
            {"completed", true},
        },
    });
    create_run_report_text("2026-01-01T120000Z_test", report.dump());

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report metrics CSV export escapes string fields", "[metrics_export][report]") {
    TempDirGuard guard;
    auto report = sample_report();
    report["emitters"] = nlohmann::json::array({
        {
            {"emitter_id", "e,1"},
            {"device_id", "dev\"0"},
            {"start_time_sec", 1.0},
            {"duration_sec", 2.0},
            {"samples_rendered", 128},
            {"peak_amplitude", 0.75},
            {"rms_amplitude", 0.25},
            {"crest_factor", 3.0},
            {"nominal_bandwidth", 1000.0},
            {"waveform_type", "cw,type"},
            {"completed", true},
        },
    });
    create_run_report_text("2026-01-01T120000Z_test", report.dump());

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "out.csv");
    REQUIRE(rc == 0);

    const std::string csv = read_file("out.csv");
    REQUIRE(csv.find("emitter_id,device_id,start_time_sec") == 0);
    REQUIRE(csv.find("\"e,1\",\"dev\"\"0\",1,2,128,0.75,0.25,3,1000,\"cw,type\",true\n") !=
            std::string::npos);
}

TEST_CASE("Report metrics export returns error when output path is a directory", "[metrics_export][report]") {
    TempDirGuard guard;
    auto report = sample_report();
    report["emitters"] = nlohmann::json::array({
        {
            {"emitter_id", "e1"},
            {"device_id", "dev0"},
            {"start_time_sec", 1.0},
            {"duration_sec", 2.0},
            {"samples_rendered", 128},
            {"peak_amplitude", 0.75},
            {"rms_amplitude", 0.25},
            {"crest_factor", 3.0},
            {"nominal_bandwidth", 1000.0},
            {"waveform_type", "cw"},
            {"completed", true},
        },
    });
    create_run_report_text("2026-01-01T120000Z_test", report.dump());
    fs::create_directories("outdir");

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "outdir");
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));
}

TEST_CASE("Report metrics export uses latest matching partial run id", "[metrics_export][report]") {
    TempDirGuard guard;

    auto old_report = sample_report();
    old_report["emitters"] = nlohmann::json::array({
        {{"emitter_id", "old"}, {"completed", true}},
    });
    create_run_report_text("2026-01-01T100000Z_test", old_report.dump());

    auto latest_report = sample_report();
    latest_report["emitters"] = nlohmann::json::array({
        {{"emitter_id", "latest"}, {"completed", true}},
    });
    create_run_report_text("2026-01-01T120000Z_test", latest_report.dump());

    CliOptions opts;
    int rc = cmd_metrics_export(opts, "2026-01-01", "out.json");
    REQUIRE(rc == 0);

    const auto exported = nlohmann::json::parse(read_file("out.json"));
    REQUIRE(exported["total_emitters"] == 1);
    REQUIRE(exported["emitters"].contains("latest"));
    REQUIRE_FALSE(exported["emitters"].contains("old"));
}

TEST_CASE("Legacy latest report metrics export uses newest run id, not newest directory mtime",
          "[metrics_export][report]") {
    TempDirGuard guard;

    auto latest_report = sample_report();
    latest_report["emitters"] = nlohmann::json::array({
        {{"emitter_id", "latest"}, {"completed", true}},
    });
    create_run_report_text("2026-01-01T120000Z_test", latest_report.dump());

    auto old_report = sample_report();
    old_report["emitters"] = nlohmann::json::array({
        {{"emitter_id", "old"}, {"completed", true}},
    });
    create_run_report_text("2026-01-01T100000Z_test", old_report.dump());

    CliOptions opts;
    int rc = cmd_metrics_export_latest(opts, "out.json");
    REQUIRE(rc == 0);

    const auto exported = nlohmann::json::parse(read_file("out.json"));
    REQUIRE(exported["total_emitters"] == 1);
    REQUIRE(exported["emitters"].contains("latest"));
    REQUIRE_FALSE(exported["emitters"].contains("old"));
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
