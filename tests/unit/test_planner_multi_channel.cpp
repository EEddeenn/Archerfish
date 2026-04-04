#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/planner.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;
using Catch::Matchers::WithinAbs;

TEST_CASE("Multi-channel plan produces channel bindings per device", "[planner][multi_channel]") {
    const std::string json_str = R"({
        "metadata": { "name": "multi_ch" },
        "devices": [
            {"id": "dev0", "channel": 0, "rf": {"freq_hz": 1e9, "rate_sps": 5e6, "gain_db": 10}},
            {"id": "dev1", "channel": 0, "rf": {"freq_hz": 2e9, "rate_sps": 10e6, "gain_db": 20}}
        ],
        "emitters": [
            {"id": "em0", "device": "dev0", "channel": 0, "start_after_sec": 0, "duration_sec": 1, "waveform": {"type": "cw", "amplitude": 0.5}},
            {"id": "em1", "device": "dev1", "channel": 0, "start_after_sec": 0, "duration_sec": 1, "waveform": {"type": "cw", "amplitude": 0.3}}
        ]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto plan_result = plan(*result);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->channels.size() == 2);
    REQUIRE(plan_result->channels[0].device_id == "dev0");
    REQUIRE(plan_result->channels[1].device_id == "dev1");
}

TEST_CASE("Multi-channel plan separates render instructions", "[planner][multi_channel]") {
    const std::string json_str = R"({
        "metadata": { "name": "multi_render" },
        "devices": [
            {"id": "dev0", "channel": 0, "rf": {"freq_hz": 1e9, "rate_sps": 5e6, "gain_db": 10}},
            {"id": "dev1", "channel": 0, "rf": {"freq_hz": 2e9, "rate_sps": 8e6, "gain_db": 15}}
        ],
        "emitters": [
            {"id": "em0", "device": "dev0", "channel": 0, "start_after_sec": 0, "duration_sec": 1, "waveform": {"type": "cw", "amplitude": 0.5}},
            {"id": "em1", "device": "dev1", "channel": 0, "start_after_sec": 0.5, "duration_sec": 0.5, "waveform": {"type": "noise", "amplitude": 0.2}}
        ]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto plan_result = plan(*result);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->render_instructions.size() == 2);
    REQUIRE_THAT(plan_result->render_instructions[0].sample_rate, WithinAbs(5e6, 1.0));
    REQUIRE_THAT(plan_result->render_instructions[1].sample_rate, WithinAbs(8e6, 1.0));
}

TEST_CASE("Multi-channel plan tracks per-channel RF settings", "[planner][multi_channel]") {
    const std::string json_str = R"({
        "metadata": { "name": "multi_rf" },
        "devices": [
            {"id": "dev0", "channel": 0, "rf": {"freq_hz": 900e6, "rate_sps": 5e6, "gain_db": 10}},
            {"id": "dev1", "channel": 0, "rf": {"freq_hz": 2400e6, "rate_sps": 20e6, "gain_db": 25}}
        ],
        "emitters": [
            {"id": "em0", "device": "dev0", "channel": 0, "start_after_sec": 0, "duration_sec": 1, "waveform": {"type": "cw", "amplitude": 0.5}},
            {"id": "em1", "device": "dev1", "channel": 0, "start_after_sec": 0, "duration_sec": 1, "waveform": {"type": "cw", "amplitude": 0.3}}
        ]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto plan_result = plan(*result);
    REQUIRE(plan_result.has_value());
    REQUIRE_THAT(plan_result->channels[0].rf.freq_hz, WithinAbs(900e6, 1.0));
    REQUIRE_THAT(plan_result->channels[1].rf.freq_hz, WithinAbs(2400e6, 1.0));
}

TEST_CASE("Multi-channel plan estimated duration covers all channels", "[planner][multi_channel]") {
    const std::string json_str = R"({
        "metadata": { "name": "multi_dur" },
        "devices": [
            {"id": "dev0", "channel": 0, "rf": {"freq_hz": 1e9, "rate_sps": 5e6, "gain_db": 10}},
            {"id": "dev1", "channel": 0, "rf": {"freq_hz": 2e9, "rate_sps": 5e6, "gain_db": 10}}
        ],
        "emitters": [
            {"id": "em0", "device": "dev0", "channel": 0, "start_after_sec": 0, "duration_sec": 3, "waveform": {"type": "cw", "amplitude": 0.5}},
            {"id": "em1", "device": "dev1", "channel": 0, "start_after_sec": 1, "duration_sec": 5, "waveform": {"type": "cw", "amplitude": 0.3}}
        ]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto plan_result = plan(*result);
    REQUIRE(plan_result.has_value());
    REQUIRE_THAT(plan_result->estimated_duration_sec, WithinAbs(6.0, 1e-12));
}

TEST_CASE("Multi-channel plan timeline includes events for all devices", "[planner][multi_channel]") {
    const std::string json_str = R"({
        "metadata": { "name": "multi_timeline" },
        "devices": [
            {"id": "dev0", "channel": 0, "rf": {"freq_hz": 1e9, "rate_sps": 5e6, "gain_db": 10}},
            {"id": "dev1", "channel": 0, "rf": {"freq_hz": 2e9, "rate_sps": 5e6, "gain_db": 10}}
        ],
        "emitters": [
            {"id": "em0", "device": "dev0", "channel": 0, "start_after_sec": 0, "duration_sec": 1, "waveform": {"type": "cw", "amplitude": 0.5}},
            {"id": "em1", "device": "dev1", "channel": 0, "start_after_sec": 0.5, "duration_sec": 1, "waveform": {"type": "cw", "amplitude": 0.3}}
        ]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto plan_result = plan(*result);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->timeline.size() == 4);
}
