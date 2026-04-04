#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/plan_io.hpp"
#include "archerfish/scenario/planner.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;
using Catch::Matchers::WithinAbs;

static Scenario make_simple_scenario() {
    Scenario s;
    s.metadata.name = "roundtrip_test";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 1.0;
    em.duration_sec = 5.0;
    em.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em);
    return s;
}

TEST_CASE("Plan to_json and from_json roundtrip preserves channels", "[plan][serialization]") {
    auto plan_result = plan(make_simple_scenario());
    REQUIRE(plan_result.has_value());
    auto j = plan_to_json(*plan_result);
    auto restored = plan_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->channels.size() == plan_result->channels.size());
    REQUIRE(restored->channels[0].device_id == "usrp0");
}

TEST_CASE("Plan roundtrip preserves timeline events", "[plan][serialization]") {
    auto plan_result = plan(make_simple_scenario());
    REQUIRE(plan_result.has_value());
    auto j = plan_to_json(*plan_result);
    auto restored = plan_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->timeline.size() == plan_result->timeline.size());
    REQUIRE(restored->timeline[0].type == TimelineEventType::EmitterStart);
    REQUIRE(restored->timeline[1].type == TimelineEventType::EmitterStop);
}

TEST_CASE("Plan roundtrip preserves render instructions", "[plan][serialization]") {
    auto plan_result = plan(make_simple_scenario());
    REQUIRE(plan_result.has_value());
    auto j = plan_to_json(*plan_result);
    auto restored = plan_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->render_instructions.size() == 1);
    REQUIRE(restored->render_instructions[0].emitter_id == "cw1");
    REQUIRE(restored->render_instructions[0].waveform.type == WaveformType::CW);
}

TEST_CASE("Plan roundtrip preserves estimated duration", "[plan][serialization]") {
    auto plan_result = plan(make_simple_scenario());
    REQUIRE(plan_result.has_value());
    auto j = plan_to_json(*plan_result);
    auto restored = plan_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE_THAT(restored->estimated_duration_sec, WithinAbs(plan_result->estimated_duration_sec, 1e-12));
}

TEST_CASE("Plan roundtrip preserves normalized scenario", "[plan][serialization]") {
    auto plan_result = plan(make_simple_scenario());
    REQUIRE(plan_result.has_value());
    auto j = plan_to_json(*plan_result);
    auto restored = plan_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->normalized_scenario.metadata.name == "roundtrip_test");
    REQUIRE(restored->normalized_scenario.devices.size() == 1);
}
