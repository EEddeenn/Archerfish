#include <catch2/catch_test_macros.hpp>
#include <archerfish/scenario/parser.hpp>
#include <archerfish/scenario/validator.hpp>

using namespace archerfish::scenario;

TEST_CASE("High power warning: gain>25 AND amplitude>0.5", "[high_power][validator]") {
    const char* json = R"({
        "metadata": {"name": "hp_test"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 2.4e9, "rate_sps": 10e6, "gain_db": 30.0}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1,
                       "waveform": {"type": "cw", "amplitude": 0.8}}]
    })";

    auto result = parse_scenario_json(json);
    REQUIRE(result.has_value());
    auto vr = validate(*result);
    REQUIRE(vr.ok());

    bool found = false;
    for (const auto& w : vr.warnings) {
        if (w.code == "W_HIGH_POWER") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("High power warning resolves waveform reference amplitude", "[high_power][validator]") {
    const char* json = R"({
        "metadata": {"name": "hp_ref_test"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 2.4e9, "rate_sps": 10e6, "gain_db": 30.0}}],
        "waveforms": [{"id": "loud", "type": "cw", "amplitude": 0.8}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1,
                       "waveform_ref": "loud"}]
    })";

    auto result = parse_scenario_json(json);
    REQUIRE(result.has_value());
    auto vr = validate(*result);
    REQUIRE(vr.ok());

    bool found = false;
    for (const auto& w : vr.warnings) {
        if (w.code == "W_HIGH_POWER") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("No high power warning when gain <= 25", "[high_power][validator]") {
    const char* json = R"({
        "metadata": {"name": "hp_ok"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 2.4e9, "rate_sps": 10e6, "gain_db": 20.0}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1,
                       "waveform": {"type": "cw", "amplitude": 0.8}}]
    })";

    auto result = parse_scenario_json(json);
    REQUIRE(result.has_value());
    auto vr = validate(*result);
    for (const auto& w : vr.warnings) {
        REQUIRE(w.code != "W_HIGH_POWER");
    }
}

TEST_CASE("No high power warning when amplitude <= 0.5", "[high_power][validator]") {
    const char* json = R"({
        "metadata": {"name": "hp_ok2"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 2.4e9, "rate_sps": 10e6, "gain_db": 30.0}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1,
                       "waveform": {"type": "cw", "amplitude": 0.3}}]
    })";

    auto result = parse_scenario_json(json);
    REQUIRE(result.has_value());
    auto vr = validate(*result);
    for (const auto& w : vr.warnings) {
        REQUIRE(w.code != "W_HIGH_POWER");
    }
}
