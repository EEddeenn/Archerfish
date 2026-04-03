#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/planner.hpp"
#include "archerfish/scenario/plan_io.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

namespace {

Scenario make_simple_scenario() {
    Scenario s;
    s.metadata.name = "test_io";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0, 8e6, "TX/RX"}});

    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 1.0;
    em.duration_sec = 5.0;
    em.waveform = WaveformDef{"cw_wf", "cw", nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em);

    return s;
}

Plan make_simple_plan() {
    auto result = plan(make_simple_scenario());
    return std::move(result.value());
}

} // namespace

TEST_CASE("plan_to_json contains expected top-level keys", "[plan_io]") {
    auto p = make_simple_plan();
    auto j = plan_to_json(p);

    REQUIRE(j.contains("normalized_scenario"));
    REQUIRE(j.contains("channels"));
    REQUIRE(j.contains("timeline"));
    REQUIRE(j.contains("render_instructions"));
    REQUIRE(j.contains("warnings"));
    REQUIRE(j.contains("estimated_duration_sec"));
}

TEST_CASE("Round-trip: Plan -> JSON -> Plan preserves all fields", "[plan_io]") {
    auto original = make_simple_plan();
    auto json = plan_to_json(original);

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());

    const auto& restored = restored_result.value();

    REQUIRE(restored.channels.size() == original.channels.size());
    REQUIRE(restored.channels[0].device_id == original.channels[0].device_id);
    REQUIRE(restored.channels[0].channel_index == original.channels[0].channel_index);
    REQUIRE_THAT(restored.channels[0].rf.freq_hz, WithinAbs(original.channels[0].rf.freq_hz, 1e-12));
    REQUIRE_THAT(restored.channels[0].rf.rate_sps, WithinAbs(original.channels[0].rf.rate_sps, 1e-12));
    REQUIRE_THAT(restored.channels[0].rf.gain_db, WithinAbs(original.channels[0].rf.gain_db, 1e-12));

    REQUIRE(restored.timeline.size() == original.timeline.size());
    for (size_t i = 0; i < original.timeline.size(); ++i) {
        REQUIRE(restored.timeline[i].type == original.timeline[i].type);
        REQUIRE_THAT(restored.timeline[i].time_sec, WithinAbs(original.timeline[i].time_sec, 1e-12));
        REQUIRE(restored.timeline[i].target_id == original.timeline[i].target_id);
    }

    REQUIRE(restored.render_instructions.size() == original.render_instructions.size());
    for (size_t i = 0; i < original.render_instructions.size(); ++i) {
        REQUIRE(restored.render_instructions[i].emitter_id == original.render_instructions[i].emitter_id);
        REQUIRE(restored.render_instructions[i].waveform.type == original.render_instructions[i].waveform.type);
        REQUIRE_THAT(restored.render_instructions[i].start_sec, WithinAbs(original.render_instructions[i].start_sec, 1e-12));
        REQUIRE_THAT(restored.render_instructions[i].duration_sec, WithinAbs(original.render_instructions[i].duration_sec, 1e-12));
        REQUIRE_THAT(restored.render_instructions[i].sample_rate, WithinAbs(original.render_instructions[i].sample_rate, 1e-12));
    }

    REQUIRE_THAT(restored.estimated_duration_sec, WithinAbs(original.estimated_duration_sec, 1e-12));

    REQUIRE(restored.warnings.size() == original.warnings.size());
    for (size_t i = 0; i < original.warnings.size(); ++i) {
        REQUIRE(restored.warnings[i].code == original.warnings[i].code);
        REQUIRE(restored.warnings[i].message == original.warnings[i].message);
    }
}

TEST_CASE("Timeline events serialize and deserialize correctly", "[plan_io]") {
    auto original = make_simple_plan();
    auto json = plan_to_json(original);

    REQUIRE(json["timeline"].is_array());
    REQUIRE(json["timeline"].size() == 2);

    REQUIRE(json["timeline"][0]["type"].get<std::string>() == "EmitterStart");
    REQUIRE_THAT(json["timeline"][0]["time_sec"].get<double>(), WithinAbs(1.0, 1e-12));
    REQUIRE(json["timeline"][0]["target_id"].get<std::string>() == "cw1");

    REQUIRE(json["timeline"][1]["type"].get<std::string>() == "EmitterStop");
    REQUIRE_THAT(json["timeline"][1]["time_sec"].get<double>(), WithinAbs(6.0, 1e-12));

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().timeline[0].type == TimelineEventType::EmitterStart);
    REQUIRE(restored_result.value().timeline[1].type == TimelineEventType::EmitterStop);
}

TEST_CASE("Render instructions without resample_ratio round-trip", "[plan_io]") {
    auto original = make_simple_plan();
    REQUIRE_FALSE(original.render_instructions[0].resample_ratio.has_value());

    auto json = plan_to_json(original);
    REQUIRE_FALSE(json["render_instructions"][0].contains("resample_ratio"));

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE_FALSE(restored_result.value().render_instructions[0].resample_ratio.has_value());
}

TEST_CASE("Render instructions with resample_ratio round-trip", "[plan_io]") {
    auto p = make_simple_plan();
    p.render_instructions[0].resample_ratio = 1.5;

    auto json = plan_to_json(p);
    REQUIRE(json["render_instructions"][0].contains("resample_ratio"));
    REQUIRE_THAT(json["render_instructions"][0]["resample_ratio"].get<double>(), WithinAbs(1.5, 1e-12));

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().render_instructions[0].resample_ratio.has_value());
    REQUIRE_THAT(*restored_result.value().render_instructions[0].resample_ratio, WithinAbs(1.5, 1e-12));
}

TEST_CASE("Warnings serialize and deserialize correctly", "[plan_io]") {
    auto p = make_simple_plan();
    p.warnings.push_back({ErrorCategory::QualityWarning, "W_TEST", "test warning"});

    auto json = plan_to_json(p);
    REQUIRE(json["warnings"].is_array());

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().warnings.size() == p.warnings.size());
    REQUIRE(restored_result.value().warnings.back().code == "W_TEST");
    REQUIRE(restored_result.value().warnings.back().message == "test warning");
}

TEST_CASE("Normalized scenario round-trips through plan JSON", "[plan_io]") {
    auto original = make_simple_plan();
    auto json = plan_to_json(original);

    REQUIRE(json["normalized_scenario"]["metadata"]["name"].get<std::string>() == "test_io");
    REQUIRE(json["normalized_scenario"]["devices"].size() == 1);
    REQUIRE(json["normalized_scenario"]["emitters"].size() == 1);

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().normalized_scenario.metadata.name == "test_io");
    REQUIRE(restored_result.value().normalized_scenario.devices.size() == 1);
    REQUIRE_THAT(restored_result.value().normalized_scenario.devices[0].rf.freq_hz,
                 WithinAbs(2.45e9, 1.0));
}

TEST_CASE("Channel bindings with optional RF fields round-trip", "[plan_io]") {
    auto original = make_simple_plan();
    REQUIRE(original.channels[0].rf.bandwidth_hz.has_value());
    REQUIRE(original.channels[0].rf.antenna.has_value());

    auto json = plan_to_json(original);
    REQUIRE(json["channels"][0]["rf"].contains("bandwidth_hz"));
    REQUIRE(json["channels"][0]["rf"].contains("antenna"));

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().channels[0].rf.bandwidth_hz.has_value());
    REQUIRE_THAT(*restored_result.value().channels[0].rf.bandwidth_hz, WithinAbs(8e6, 1.0));
    REQUIRE(restored_result.value().channels[0].rf.antenna.has_value());
    REQUIRE(*restored_result.value().channels[0].rf.antenna == "TX/RX");
}
