#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/app.hpp>
#include <archerfish/cli/cmd_dryrun.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using namespace archerfish::cli;

namespace {

struct TempDirGuard {
    fs::path original;
    fs::path temp_dir;

    TempDirGuard() {
        original = fs::current_path();
        temp_dir = fs::temp_directory_path() / ("archerfish_test_dryrun_" + std::to_string(::getpid()));
        fs::create_directories(temp_dir);
        fs::current_path(temp_dir);
    }

    ~TempDirGuard() {
        fs::current_path(original);
        fs::remove_all(temp_dir);
    }
};

std::string capture_stdout(const std::function<int()>& fn, int& rc) {
    int pipefd[2];
    REQUIRE(::pipe(pipefd) == 0);

    int saved_stdout = ::dup(STDOUT_FILENO);
    REQUIRE(saved_stdout != -1);
    REQUIRE(::dup2(pipefd[1], STDOUT_FILENO) != -1);
    ::close(pipefd[1]);

    rc = fn();
    std::fflush(stdout);

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

void write_dryrun_scenario(const fs::path& path) {
    std::ofstream f(path);
    f << R"({
  "metadata": { "name": "dryrun_chirp" },
  "devices": [
    {
      "id": "usrp0",
      "channel": 0,
      "rf": {
        "freq_hz": 915000000.0,
        "rate_sps": 20000000.0,
        "gain_db": 18.0
      }
    }
  ],
  "emitters": [
    {
      "id": "chirp1",
      "device": "usrp0",
      "channel": 0,
      "start_after_sec": 1.0,
      "duration_sec": 0.02,
      "waveform": {
        "type": "chirp",
        "f0_hz": -2000000.0,
        "f1_hz": 2000000.0,
        "amplitude": 0.3
      }
    }
  ]
})";
}

void write_dryrun_scenario_with_bad_display_params(const fs::path& path) {
    std::ofstream f(path);
    f << R"({
  "metadata": { "name": "dryrun_bad_display_params" },
  "devices": [
    {
      "id": "usrp0",
      "channel": 0,
      "rf": {
        "freq_hz": 915000000.0,
        "rate_sps": 20000000.0,
        "gain_db": 18.0
      }
    }
  ],
  "emitters": [
    {
      "id": "chirp1",
      "device": "usrp0",
      "channel": 0,
      "start_after_sec": 1.0,
      "duration_sec": 0.02,
      "waveform": {
        "type": "chirp",
        "f0_hz": "not a number",
        "f1_hz": 2000000.0,
        "amplitude": 0.3
      }
    }
  ]
})";
}

struct TestEmitter {
    std::string id;
    std::string device_id;
    uint32_t channel{0};
    double start_sec{0.0};
    double duration_sec{0.0};
    std::string waveform_type;
};

struct TestDevice {
    std::string id;
    uint32_t channel{0};
    double freq_hz{0.0};
    double rate_sps{0.0};
    double gain_db{0.0};
};

struct TestScenario {
    std::string name;
    std::vector<TestDevice> devices;
    std::vector<TestEmitter> emitters;
    double duration_sec{0.0};
};

std::string generate_test_timeline(const TestScenario& scenario) {
    std::ostringstream out;

    out << fmt::format("Scenario: \"{}\" | {} device{} | {} emitter{} | Duration: {:.1f}s\n",
                       scenario.name,
                       scenario.devices.size(),
                       scenario.devices.size() != 1 ? "s" : "",
                       scenario.emitters.size(),
                       scenario.emitters.size() != 1 ? "s" : "",
                       scenario.duration_sec);

    std::map<std::string, std::vector<TestEmitter>> device_entries;
    for (const auto& e : scenario.emitters) {
        device_entries[e.device_id].push_back(e);
    }

    for (auto& [dev_id, entries] : device_entries) {
        std::sort(entries.begin(), entries.end(),
                  [](const TestEmitter& a, const TestEmitter& b) {
                      return a.start_sec < b.start_sec;
                  });
    }

    for (const auto& [dev_id, entries] : device_entries) {
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            bool is_last = (i == entries.size() - 1);
            (void)is_last;

            out << fmt::format("  {} [{:.3f}s --- {:.3f}s] {}\n",
                               e.id, e.start_sec, e.start_sec + e.duration_sec,
                               e.waveform_type);
        }
    }

    return out.str();
}

} // namespace

TEST_CASE("Dry-run command emits readable text timeline", "[dryrun]") {
    TempDirGuard guard;
    write_dryrun_scenario("scenario.json");

    CliOptions opts;
    opts.json_output = false;

    int rc = -1;
    auto output = capture_stdout([&]() {
        return cmd_dryrun(opts, "scenario.json");
    }, rc);

    REQUIRE(rc == 0);
    REQUIRE_FALSE(output.empty());
    CHECK(output.find("dryrun_chirp") != std::string::npos);
    CHECK(output.find("chirp1") != std::string::npos);
    CHECK(output.find("Chirp f0=-2.0 MHz -> 2.0 MHz") != std::string::npos);
    CHECK(output.find("---") != std::string::npos);
    CHECK(std::all_of(output.begin(), output.end(), [](unsigned char c) {
        return c < 0x80;
    }));
}

TEST_CASE("Dry-run text output tolerates malformed optional display parameters", "[dryrun]") {
    TempDirGuard guard;
    write_dryrun_scenario_with_bad_display_params("scenario.json");

    CliOptions opts;
    opts.json_output = false;

    int rc = -1;
    auto output = capture_stdout([&]() {
        return cmd_dryrun(opts, "scenario.json");
    }, rc);

    REQUIRE(rc == 0);
    CHECK(output.find("dryrun_bad_display_params") != std::string::npos);
    CHECK(output.find("Chirp f0=0 Hz -> 2.0 MHz") != std::string::npos);
}

TEST_CASE("Dry-run timeline generation with single device and emitter", "[dryrun]") {
    TestScenario scenario;
    scenario.name = "cw_test";
    scenario.devices = {{"usrp0", 0, 2.4e9, 10e6, 20.0}};
    scenario.emitters = {{"cw1", "usrp0", 0, 0.0, 1.0, "cw"}};
    scenario.duration_sec = 1.0;

    auto result = generate_test_timeline(scenario);

    REQUIRE_FALSE(result.empty());
    CHECK(result.find("cw_test") != std::string::npos);
    CHECK(result.find("1 device") != std::string::npos);
    CHECK(result.find("1 emitter") != std::string::npos);
    CHECK(result.find("cw1") != std::string::npos);
    CHECK(result.find("cw") != std::string::npos);
}

TEST_CASE("Dry-run timeline generation with multiple devices", "[dryrun]") {
    TestScenario scenario;
    scenario.name = "multi_device";
    scenario.devices = {
        {"usrp0", 0, 2.4e9, 10e6, 20.0},
        {"usrp1", 0, 5.8e9, 20e6, 15.0},
    };
    scenario.emitters = {
        {"cw1", "usrp0", 0, 0.0, 1.0, "cw"},
        {"chirp1", "usrp0", 0, 0.5, 1.0, "chirp"},
        {"pulse1", "usrp1", 0, 0.0, 2.0, "pulse"},
    };
    scenario.duration_sec = 2.0;

    auto result = generate_test_timeline(scenario);

    REQUIRE_FALSE(result.empty());
    CHECK(result.find("multi_device") != std::string::npos);
    CHECK(result.find("2 devices") != std::string::npos);
    CHECK(result.find("3 emitters") != std::string::npos);
    CHECK(result.find("cw1") != std::string::npos);
    CHECK(result.find("chirp1") != std::string::npos);
    CHECK(result.find("pulse1") != std::string::npos);
}

TEST_CASE("Dry-run timeline includes timing information", "[dryrun]") {
    TestScenario scenario;
    scenario.name = "timing_test";
    scenario.devices = {{"usrp0", 0, 1e9, 10e6, 10.0}};
    scenario.emitters = {{"e1", "usrp0", 0, 0.5, 1.5, "cw"}};
    scenario.duration_sec = 2.0;

    auto result = generate_test_timeline(scenario);

    CHECK(result.find("0.500") != std::string::npos);
    CHECK(result.find("2.000") != std::string::npos);
    CHECK(result.find("e1") != std::string::npos);
}

TEST_CASE("Dry-run timeline with empty emitters", "[dryrun]") {
    TestScenario scenario;
    scenario.name = "empty";
    scenario.devices = {{"usrp0", 0, 1e9, 10e6, 10.0}};
    scenario.emitters = {};
    scenario.duration_sec = 0.0;

    auto result = generate_test_timeline(scenario);

    REQUIRE_FALSE(result.empty());
    CHECK(result.find("empty") != std::string::npos);
    CHECK(result.find("0 emitters") != std::string::npos);
}

TEST_CASE("Dry-run JSON output structure", "[dryrun]") {
    nlohmann::json out;
    out["scenario"] = "test_scenario";
    out["devices"] = 2;
    out["emitters"] = 3;
    out["duration_sec"] = 2.0;
    out["render_instructions"] = nlohmann::json::array();
    out["render_instructions"].push_back({
        {"emitter_id", "cw1"},
        {"start_sec", 0.0},
        {"duration_sec", 1.0},
        {"waveform_type", "cw"},
    });

    auto json_str = out.dump(2);
    auto parsed = nlohmann::json::parse(json_str);

    CHECK(parsed["scenario"] == "test_scenario");
    CHECK(parsed["devices"] == 2);
    CHECK(parsed["emitters"] == 3);
    CHECK(parsed["duration_sec"] == 2.0);
    REQUIRE(parsed["render_instructions"].size() == 1);
    CHECK(parsed["render_instructions"][0]["emitter_id"] == "cw1");
    CHECK(parsed["render_instructions"][0]["waveform_type"] == "cw");
}

TEST_CASE("Dry-run pluralization correctness", "[dryrun]") {
    TestScenario single;
    single.name = "single";
    single.devices = {{"usrp0", 0, 1e9, 10e6, 10.0}};
    single.emitters = {{"e1", "usrp0", 0, 0.0, 1.0, "cw"}};
    single.duration_sec = 1.0;

    auto single_result = generate_test_timeline(single);
    CHECK(single_result.find("1 device") != std::string::npos);
    CHECK(single_result.find("1 emitter") != std::string::npos);

    TestScenario plural;
    plural.name = "plural";
    plural.devices = {{"usrp0", 0, 1e9, 10e6, 10.0}, {"usrp1", 0, 2e9, 20e6, 15.0}};
    plural.emitters = {{"e1", "usrp0", 0, 0.0, 1.0, "cw"}, {"e2", "usrp1", 0, 0.0, 1.0, "chirp"}};
    plural.duration_sec = 1.0;

    auto plural_result = generate_test_timeline(plural);
    CHECK(plural_result.find("2 devices") != std::string::npos);
    CHECK(plural_result.find("2 emitters") != std::string::npos);
}
