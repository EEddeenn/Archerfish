#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/cmd_scenario.hpp>
#include <archerfish/reporting/metrics.hpp>
#include <archerfish/reporting/report.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <unistd.h>

#include <nlohmann/json.hpp>

using namespace archerfish::cli;
namespace fs = std::filesystem;

static const char* examples_dir = EXAMPLES_DIR;

namespace {

std::string capture_stdout(const std::function<int()>& fn, int& rc) {
    std::fflush(nullptr);

    int pipefd[2];
    REQUIRE(::pipe(pipefd) == 0);

    int saved_stdout = ::dup(STDOUT_FILENO);
    REQUIRE(saved_stdout != -1);
    REQUIRE(::dup2(pipefd[1], STDOUT_FILENO) != -1);
    ::close(pipefd[1]);

    rc = fn();
    std::fflush(nullptr);

    REQUIRE(::dup2(saved_stdout, STDOUT_FILENO) != -1);
    ::close(saved_stdout);

    std::string output;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = ::read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, static_cast<size_t>(bytes_read));
    }
    ::close(pipefd[0]);

    return output;
}

struct TempDirGuard {
    fs::path original;
    fs::path temp_dir;

    explicit TempDirGuard(const std::string& prefix) {
        original = fs::current_path();
        temp_dir = fs::temp_directory_path() / (prefix + "_" + std::to_string(::getpid()));
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);
        fs::current_path(temp_dir);
    }

    ~TempDirGuard() {
        fs::current_path(original);
        fs::remove_all(temp_dir);
    }
};

void write_file(const fs::path& path, const std::string& content) {
    std::ofstream f(path);
    REQUIRE(f);
    f << content;
    REQUIRE(f);
}

nlohmann::json read_json_file(const fs::path& path) {
    std::ifstream f(path);
    REQUIRE(f);
    return nlohmann::json::parse(f);
}

std::vector<fs::path> list_directories(const fs::path& path) {
    std::vector<fs::path> dirs;
    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_directory()) {
            dirs.push_back(entry.path());
        }
    }
    return dirs;
}

bool artifact_paths_contain_filename(const nlohmann::json& artifact_paths,
                                     const std::string& filename) {
    for (const auto& path : artifact_paths) {
        REQUIRE(path.is_string());
        if (fs::path(path.get<std::string>()).filename() == filename) {
            return true;
        }
    }
    return false;
}

} // namespace

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

TEST_CASE("scenario run rejects blank device id", "[cli][scenario]") {
    CliOptions opts;
    opts.device_id = " \t\n";
    std::string path = std::string(examples_dir) + "/future_start_cw.json";
    int rc = cmd_scenario_run(opts, path);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
}

TEST_CASE("scenario run accepts explicit stub device id", "[cli][scenario]") {
    CliOptions opts;
    opts.device_id = "stub0";
    std::string path = std::string(examples_dir) + "/future_start_cw.json";
    int rc = cmd_scenario_run(opts, path);
    REQUIRE(rc == 0);
}

TEST_CASE("scenario run --json emits parseable JSON only", "[cli][scenario]") {
    CliOptions opts;
    opts.json_output = true;
    opts.device_id = "stub0";
    std::string path = std::string(examples_dir) + "/future_start_cw.json";

    int rc = -1;
    auto output = capture_stdout([&]() {
        return cmd_scenario_run(opts, path);
    }, rc);

    REQUIRE(rc == 0);
    auto parsed = nlohmann::json::parse(output);
    REQUIRE(parsed["device"] == "stub0");
    REQUIRE(parsed["samples_sent"].is_number_unsigned());
}

TEST_CASE("scenario run writes requested reporting artifacts", "[cli][scenario]") {
    TempDirGuard guard("archerfish_test_scenario_artifacts");
    write_file("scenario.json", R"({
  "metadata": { "name": "reported_run" },
  "devices": [
    {
      "id": "usrp0",
      "channel": 0,
      "rf": {
        "freq_hz": 915000000.0,
        "rate_sps": 1000000.0,
        "gain_db": 0.0
      }
    }
  ],
  "emitters": [
    {
      "id": "cw1",
      "device": "usrp0",
      "channel": 0,
      "start_after_sec": 0.0,
      "duration_sec": 0.001,
      "waveform": {
        "type": "cw",
        "amplitude": 0.1
      }
    }
  ],
  "reporting": {
    "save_plan": true,
    "save_metrics": true
  }
})");

    CliOptions opts;
    opts.device_id = "stub0";

    int rc = cmd_scenario_run(opts, "scenario.json");
    REQUIRE(rc == 0);

    REQUIRE(fs::is_directory("runs"));
    const auto run_dirs = list_directories("runs");
    REQUIRE(run_dirs.size() == 1);
    const auto& run_dir = run_dirs.front();

    REQUIRE(fs::is_regular_file(run_dir / "scenario.normalized.json"));
    REQUIRE(fs::is_regular_file(run_dir / "plan.json"));
    REQUIRE(fs::is_regular_file(run_dir / "metrics.json"));
    REQUIRE(fs::is_regular_file(run_dir / "report.json"));
    REQUIRE(fs::is_regular_file(run_dir / "logs.txt"));

    const auto metrics_json = read_json_file(run_dir / "metrics.json");
    const auto metrics = archerfish::reporting::Metrics::from_json(metrics_json);
    REQUIRE(metrics.total_samples_sent > 0);

    const auto report_json = read_json_file(run_dir / "report.json");
    const auto report = archerfish::reporting::Report::from_json(report_json);
    REQUIRE(report.scenario_name == "reported_run");
    REQUIRE(report.status == "completed");
    REQUIRE(report.devices.size() == 1);
    REQUIRE(report.devices.front().device_id == "usrp0");
    REQUIRE(report.devices.front().device_type == "stub");
    REQUIRE(report.devices.front().channel == 0);

    REQUIRE(artifact_paths_contain_filename(report_json["artifact_paths"], "scenario.normalized.json"));
    REQUIRE(artifact_paths_contain_filename(report_json["artifact_paths"], "plan.json"));
    REQUIRE(artifact_paths_contain_filename(report_json["artifact_paths"], "metrics.json"));
    REQUIRE(artifact_paths_contain_filename(report_json["artifact_paths"], "report.json"));
}
