#include <catch2/catch_test_macros.hpp>
#include <archerfish/scenario/scenario.hpp>
#include <archerfish/scenario/parser.hpp>
#include <archerfish/scenario/planner.hpp>

using namespace archerfish::scenario;

TEST_CASE("Replay mode parsed from JSON", "[replay][parser]") {
    const char* json = R"({
        "metadata": {"name": "replay_test"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1, "waveform": {"type": "cw", "amplitude": 0.5}}],
        "run": {"mode": "replay"}
    })";

    auto result = parse_scenario_json(json);
    REQUIRE(result.has_value());
    REQUIRE(result->run.mode == RunMode::Replay);
}

TEST_CASE("Realtime mode is default when run not specified", "[replay][parser]") {
    const char* json = R"({
        "metadata": {"name": "rt_test"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1, "waveform": {"type": "cw", "amplitude": 0.5}}]
    })";

    auto result = parse_scenario_json(json);
    REQUIRE(result.has_value());
    REQUIRE(result->run.mode == RunMode::Realtime);
}

TEST_CASE("Replay mode propagated through planner", "[replay][planner]") {
    const char* json = R"({
        "metadata": {"name": "replay_plan"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1, "waveform": {"type": "cw", "amplitude": 0.5}}],
        "run": {"mode": "replay"}
    })";

    auto scenario = parse_scenario_json(json);
    REQUIRE(scenario.has_value());
    auto plan_result = plan(*scenario);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->run_mode == RunMode::Replay);
}

TEST_CASE("Realtime mode propagated through planner by default", "[replay][planner]") {
    const char* json = R"({
        "metadata": {"name": "rt_plan"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1, "waveform": {"type": "cw", "amplitude": 0.5}}]
    })";

    auto scenario = parse_scenario_json(json);
    REQUIRE(scenario.has_value());
    auto plan_result = plan(*scenario);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->run_mode == RunMode::Realtime);
}
