#include <catch2/catch_test_macros.hpp>
#include <archerfish/scenario/parser.hpp>
#include <archerfish/scenario/validator.hpp>

using namespace archerfish::scenario;

TEST_CASE("target_power_dbm parsed in waveform", "[target_power][parser]") {
    const char* json = R"({
        "metadata": {"name": "tp_test"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1,
                       "waveform": {"type": "cw", "amplitude": 0.5, "target_power_dbm": -10.0}}]
    })";

    auto result = parse_scenario_json(json);
    REQUIRE(result.has_value());
    const auto& em = result->emitters[0];
    REQUIRE(em.waveform.has_value());
    REQUIRE(em.waveform->target_power_dbm.has_value());
    REQUIRE(em.waveform->target_power_dbm.value() == -10.0);
}

TEST_CASE("target_power_dbm in waveforms array", "[target_power][parser]") {
    const char* json = R"({
        "metadata": {"name": "tp_wf"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20}}],
        "waveforms": [{"id": "wf0", "type": "cw", "amplitude": 0.3, "target_power_dbm": -5.0}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1,
                       "waveform_ref": "wf0"}]
    })";

    auto result = parse_scenario_json(json);
    REQUIRE(result.has_value());
    REQUIRE(result->waveforms[0].target_power_dbm.has_value());
    REQUIRE(result->waveforms[0].target_power_dbm.value() == -5.0);
}

TEST_CASE("target_power_dbm is optional", "[target_power][parser]") {
    const char* json = R"({
        "metadata": {"name": "tp_none"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1,
                       "waveform": {"type": "cw", "amplitude": 0.5}}]
    })";

    auto result = parse_scenario_json(json);
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->emitters[0].waveform->target_power_dbm.has_value());
}
