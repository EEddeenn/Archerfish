#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/parser.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;

TEST_CASE("Waveform ref resolves inline waveform untouched", "[parser][waveform_ref]") {
    const std::string json_str = R"({
        "metadata": { "name": "inline_untouched" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform": {"type": "cw", "amplitude": 0.7}}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto errors = resolve_waveform_refs(*result);
    REQUIRE(errors.empty());
    REQUIRE(result->emitters[0].waveform.has_value());
    REQUIRE(result->emitters[0].waveform->type == WaveformType::CW);
}

TEST_CASE("Waveform ref resolves named waveform correctly", "[parser][waveform_ref]") {
    const std::string json_str = R"({
        "metadata": { "name": "named_ref" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "waveforms": [{"id": "wf1", "type": "noise", "amplitude": 0.15}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform_ref": "wf1"}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->emitters[0].waveform.has_value());
    auto errors = resolve_waveform_refs(*result);
    REQUIRE(errors.empty());
    REQUIRE(result->emitters[0].waveform.has_value());
    REQUIRE_FALSE(result->emitters[0].waveform_ref.has_value());
    REQUIRE(result->emitters[0].waveform->type == WaveformType::Noise);
    REQUIRE(result->emitters[0].waveform->params["amplitude"].get<double>() == 0.15);
}

TEST_CASE("Waveform ref to nonexistent waveform returns error", "[parser][waveform_ref]") {
    const std::string json_str = R"({
        "metadata": { "name": "dangling_ref" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform_ref": "ghost"}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto errors = resolve_waveform_refs(*result);
    REQUIRE_FALSE(errors.empty());
    REQUIRE(errors[0].code == "E_UNRESOLVED_REF");
}

TEST_CASE("Multiple waveform refs resolve independently", "[parser][waveform_ref]") {
    const std::string json_str = R"({
        "metadata": { "name": "multi_ref" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "waveforms": [
            {"id": "wf_a", "type": "cw", "amplitude": 0.1},
            {"id": "wf_b", "type": "noise", "amplitude": 0.2}
        ],
        "emitters": [
            {"id": "em0", "device": "usrp0", "start_after_sec": 0, "duration_sec": 1, "waveform_ref": "wf_a"},
            {"id": "em1", "device": "usrp0", "start_after_sec": 1, "duration_sec": 1, "waveform_ref": "wf_b"}
        ]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto errors = resolve_waveform_refs(*result);
    REQUIRE(errors.empty());
    REQUIRE(result->emitters[0].waveform->type == WaveformType::CW);
    REQUIRE(result->emitters[1].waveform->type == WaveformType::Noise);
    REQUIRE_FALSE(result->emitters[0].waveform_ref.has_value());
    REQUIRE_FALSE(result->emitters[1].waveform_ref.has_value());
}

TEST_CASE("Emitter with both waveform and waveform_ref keeps inline", "[parser][waveform_ref]") {
    const std::string json_str = R"({
        "metadata": { "name": "both_wf" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "waveforms": [{"id": "wf_a", "type": "noise", "amplitude": 0.3}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform": {"type": "cw", "amplitude": 0.5}, "waveform_ref": "wf_a"}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto errors = resolve_waveform_refs(*result);
    REQUIRE(errors.empty());
    REQUIRE(result->emitters[0].waveform->type == WaveformType::CW);
}
