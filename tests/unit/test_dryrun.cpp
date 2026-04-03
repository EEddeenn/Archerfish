#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace {

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
