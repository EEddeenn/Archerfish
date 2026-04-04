#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/plan_io.hpp"
#include "archerfish/scenario/parser.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;
using Catch::Matchers::WithinAbs;

TEST_CASE("Scenario to_json and from_json roundtrip preserves metadata", "[scenario][roundtrip]") {
    const std::string json_str = R"({
        "metadata": { "name": "rt_test", "description": "a test", "version": "1.0" },
        "devices": [{"id": "usrp0", "channel": 0, "rf": {"freq_hz": 2.45e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{"id": "em0", "device": "usrp0", "channel": 0, "start_after_sec": 1, "duration_sec": 5, "waveform": {"type": "cw", "amplitude": 0.5}}],
        "reporting": {"save_plan": true, "save_metrics": false}
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto j = scenario_to_json(*result);
    auto restored = scenario_from_json(j);
    REQUIRE(restored.has_value());
    CHECK(restored->metadata.name == "rt_test");
    CHECK(restored->metadata.description.has_value());
    CHECK(*restored->metadata.description == "a test");
    CHECK(restored->metadata.version.has_value());
    CHECK(*restored->metadata.version == "1.0");
}

TEST_CASE("Scenario roundtrip preserves devices", "[scenario][roundtrip]") {
    const std::string json_str = R"({
        "metadata": { "name": "dev_rt" },
        "devices": [{"id": "usrp0", "channel": 0, "rf": {"freq_hz": 1e9, "rate_sps": 5e6, "gain_db": 10}}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform": {"type": "cw", "amplitude": 0.5}}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto j = scenario_to_json(*result);
    auto restored = scenario_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->devices.size() == 1);
    CHECK(restored->devices[0].id == "usrp0");
    CHECK_THAT(restored->devices[0].rf.freq_hz, WithinAbs(1e9, 1.0));
    CHECK_THAT(restored->devices[0].rf.rate_sps, WithinAbs(5e6, 1.0));
    CHECK_THAT(restored->devices[0].rf.gain_db, WithinAbs(10.0, 1e-12));
}

TEST_CASE("Scenario roundtrip preserves emitters with waveforms", "[scenario][roundtrip]") {
    const std::string json_str = R"({
        "metadata": { "name": "em_rt" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "usrp0", "start_after_sec": 0.5, "duration_sec": 2.0, "waveform": {"type": "noise", "amplitude": 0.1}}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto j = scenario_to_json(*result);
    auto restored = scenario_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->emitters.size() == 1);
    CHECK(restored->emitters[0].id == "em0");
    CHECK(restored->emitters[0].device == "usrp0");
    CHECK_THAT(restored->emitters[0].start_after_sec, WithinAbs(0.5, 1e-12));
    CHECK_THAT(restored->emitters[0].duration_sec, WithinAbs(2.0, 1e-12));
    REQUIRE(restored->emitters[0].waveform.has_value());
    CHECK(restored->emitters[0].waveform->type == WaveformType::Noise);
}

TEST_CASE("Scenario roundtrip preserves reporting config", "[scenario][roundtrip]") {
    const std::string json_str = R"({
        "metadata": { "name": "rpt_rt" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform": {"type": "cw", "amplitude": 0.5}}],
        "reporting": {"save_plan": true, "save_metrics": true}
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto j = scenario_to_json(*result);
    auto restored = scenario_from_json(j);
    REQUIRE(restored.has_value());
    CHECK(restored->reporting.save_plan);
    CHECK(restored->reporting.save_metrics);
}

TEST_CASE("Scenario roundtrip with named waveforms", "[scenario][roundtrip]") {
    const std::string json_str = R"({
        "metadata": { "name": "wf_rt" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "waveforms": [{"id": "my_cw", "type": "cw", "amplitude": 0.5}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform_ref": "my_cw"}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto j = scenario_to_json(*result);
    auto restored = scenario_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->waveforms.size() == 1);
    CHECK(restored->waveforms[0].id.has_value());
    CHECK(*restored->waveforms[0].id == "my_cw");
    CHECK(restored->waveforms[0].type == WaveformType::CW);
}
