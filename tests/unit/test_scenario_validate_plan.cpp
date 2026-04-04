#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"

using namespace archerfish::scenario;
using Catch::Matchers::WithinAbs;

TEST_CASE("End-to-end: valid CW scenario parse -> validate -> plan", "[integration][pipeline]") {
    const std::string json_str = R"({
        "metadata": { "name": "e2e_cw" },
        "devices": [{"id": "usrp0", "channel": 0, "rf": {"freq_hz": 2.45e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{"id": "cw1", "device": "usrp0", "channel": 0, "start_after_sec": 0.5, "duration_sec": 2.0,
                       "waveform": {"type": "cw", "amplitude": 0.5}}]
    })";
    auto parse_result = parse_scenario_json(json_str);
    REQUIRE(parse_result.has_value());

    auto validation = validate(*parse_result);
    REQUIRE(validation.ok());

    auto plan_result = plan(*parse_result);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->render_instructions.size() == 1);
    REQUIRE_THAT(plan_result->estimated_duration_sec, WithinAbs(2.5, 1e-12));
}

TEST_CASE("End-to-end: invalid scenario fails validation", "[integration][pipeline]") {
    const std::string json_str = R"({
        "metadata": { "name": "bad_scenario" },
        "devices": [{"id": "usrp0", "channel": 0, "rf": {"freq_hz": 2.45e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{"id": "cw1", "device": "usrp0", "channel": 0, "start_after_sec": 0, "duration_sec": 1.0,
                       "waveform": {"type": "cw", "amplitude": 2.0}}]
    })";
    auto parse_result = parse_scenario_json(json_str);
    REQUIRE(parse_result.has_value());

    auto validation = validate(*parse_result);
    REQUIRE_FALSE(validation.ok());
}

TEST_CASE("End-to-end: multi-emitter scenario produces correct plan", "[integration][pipeline]") {
    const std::string json_str = R"({
        "metadata": { "name": "e2e_multi" },
        "devices": [{"id": "usrp0", "channel": 0, "rf": {"freq_hz": 1e9, "rate_sps": 5e6, "gain_db": 10}}],
        "emitters": [
            {"id": "em0", "device": "usrp0", "channel": 0, "start_after_sec": 0, "duration_sec": 2, "waveform": {"type": "cw", "amplitude": 0.5}},
            {"id": "em1", "device": "usrp0", "channel": 0, "start_after_sec": 2, "duration_sec": 3, "waveform": {"type": "noise", "amplitude": 0.2}}
        ]
    })";
    auto parse_result = parse_scenario_json(json_str);
    REQUIRE(parse_result.has_value());

    auto validation = validate(*parse_result);
    REQUIRE(validation.ok());

    auto plan_result = plan(*parse_result);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->render_instructions.size() == 2);
    REQUIRE_THAT(plan_result->estimated_duration_sec, WithinAbs(5.0, 1e-12));
}

TEST_CASE("End-to-end: scenario with waveform ref resolves and plans", "[integration][pipeline]") {
    const std::string json_str = R"({
        "metadata": { "name": "e2e_ref" },
        "devices": [{"id": "usrp0", "channel": 0, "rf": {"freq_hz": 1e9, "rate_sps": 5e6, "gain_db": 10}}],
        "waveforms": [{"id": "my_cw", "type": "cw", "amplitude": 0.5}],
        "emitters": [{"id": "em0", "device": "usrp0", "channel": 0, "start_after_sec": 0, "duration_sec": 1,
                       "waveform_ref": "my_cw"}]
    })";
    auto parse_result = parse_scenario_json(json_str);
    REQUIRE(parse_result.has_value());

    auto errors = resolve_waveform_refs(*parse_result);
    REQUIRE(errors.empty());

    auto validation = validate(*parse_result);
    REQUIRE(validation.ok());

    auto plan_result = plan(*parse_result);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->render_instructions[0].waveform.type == archerfish::dsp::WaveformType::CW);
}
