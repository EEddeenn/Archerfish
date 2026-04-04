#include "archerfish/scenario/planner.hpp"
#include "archerfish/dsp/waveform_type.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nlohmann/json.hpp>

using archerfish::dsp::WaveformType;
using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

namespace {

Scenario make_two_channel_scenario() {
    Scenario s;
    s.metadata.name = "two_channel";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});

    ChannelDef ch0;
    ch0.id = "ch0";
    ch0.device = "usrp0";
    ch0.index = 0;
    ch0.rf = {2.45e9, 10e6, 20.0};
    s.channel_defs.push_back(ch0);

    ChannelDef ch1;
    ch1.id = "ch1";
    ch1.device = "usrp0";
    ch1.index = 1;
    ch1.rf = {2.45e9, 20e6, 18.0};
    s.channel_defs.push_back(ch1);

    EmitterDef em0;
    em0.id = "cw0";
    em0.device = "usrp0";
    em0.channel = 0;
    em0.channel_id = "ch0";
    em0.start_after_sec = 1.0;
    em0.duration_sec = 5.0;
    em0.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em0);

    EmitterDef em1;
    em1.id = "cw1";
    em1.device = "usrp0";
    em1.channel = 1;
    em1.channel_id = "ch1";
    em1.start_after_sec = 0.5;
    em1.duration_sec = 3.0;
    em1.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.3}}};
    s.emitters.push_back(em1);

    return s;
}

Scenario make_two_channel_by_device_channel_scenario() {
    Scenario s;
    s.metadata.name = "two_channel_implicit";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});

    ChannelDef ch0;
    ch0.id = "ch0";
    ch0.device = "usrp0";
    ch0.index = 0;
    ch0.rf = {2.45e9, 10e6, 20.0};
    s.channel_defs.push_back(ch0);

    ChannelDef ch1;
    ch1.id = "ch1";
    ch1.device = "usrp0";
    ch1.index = 1;
    ch1.rf = {2.45e9, 20e6, 18.0};
    s.channel_defs.push_back(ch1);

    EmitterDef em0;
    em0.id = "cw0";
    em0.device = "usrp0";
    em0.channel = 0;
    em0.start_after_sec = 1.0;
    em0.duration_sec = 5.0;
    em0.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em0);

    EmitterDef em1;
    em1.id = "cw1";
    em1.device = "usrp0";
    em1.channel = 1;
    em1.start_after_sec = 0.5;
    em1.duration_sec = 3.0;
    em1.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.3}}};
    s.emitters.push_back(em1);

    return s;
}

Scenario make_two_channel_with_events_scenario() {
    Scenario s;
    s.metadata.name = "two_channel_events";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});

    ChannelDef ch0;
    ch0.id = "ch0";
    ch0.device = "usrp0";
    ch0.index = 0;
    ch0.rf = {2.45e9, 10e6, 20.0};
    s.channel_defs.push_back(ch0);

    ChannelDef ch1;
    ch1.id = "ch1";
    ch1.device = "usrp0";
    ch1.index = 1;
    ch1.rf = {2.45e9, 20e6, 18.0};
    s.channel_defs.push_back(ch1);

    EmitterDef em0;
    em0.id = "cw0";
    em0.device = "usrp0";
    em0.channel = 0;
    em0.channel_id = "ch0";
    em0.start_after_sec = 1.0;
    em0.duration_sec = 5.0;
    em0.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em0);

    EmitterDef em1;
    em1.id = "cw1";
    em1.device = "usrp0";
    em1.channel = 1;
    em1.channel_id = "ch1";
    em1.start_after_sec = 0.5;
    em1.duration_sec = 3.0;
    em1.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.3}}};
    s.emitters.push_back(em1);

    ScenarioEvent gain_evt;
    gain_evt.target_device = "usrp0";
    gain_evt.time_sec = 2.0;
    gain_evt.type = "gain_change";
    gain_evt.payload = {{"gain_db", 25.0}};
    s.events.push_back(gain_evt);

    ScenarioEvent marker_evt;
    marker_evt.target_device = "usrp0";
    marker_evt.time_sec = 4.0;
    marker_evt.type = "marker";
    marker_evt.payload = {{"label", "checkpoint"}};
    s.events.push_back(marker_evt);

    return s;
}

} // namespace

TEST_CASE("Multi-channel planner creates per-channel plans from explicit channels", "[planner][multi_channel]") {
    auto result = plan(make_two_channel_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.channel_plans.size() == 2);

    REQUIRE(p.channel_plans[0].channel_id == "ch0");
    REQUIRE(p.channel_plans[0].channel_index == 0);
    REQUIRE_THAT(p.channel_plans[0].rf.freq_hz, WithinAbs(2.45e9, 1.0));
    REQUIRE_THAT(p.channel_plans[0].rf.rate_sps, WithinAbs(10e6, 1.0));

    REQUIRE(p.channel_plans[1].channel_id == "ch1");
    REQUIRE(p.channel_plans[1].channel_index == 1);
    REQUIRE_THAT(p.channel_plans[1].rf.freq_hz, WithinAbs(2.45e9, 1.0));
    REQUIRE_THAT(p.channel_plans[1].rf.rate_sps, WithinAbs(20e6, 1.0));
}

TEST_CASE("Multi-channel planner backward compatibility (no channel_defs → auto-generated channel_plans)", "[planner][multi_channel]") {
    Scenario s;
    s.metadata.name = "simple_compat";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});

    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 1.0;
    em.duration_sec = 5.0;
    em.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em);

    auto result = plan(s);
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.channel_plans.size() == 1);
    REQUIRE(p.channel_plans[0].channel_id == "usrp0_ch0");
    REQUIRE(p.channel_plans[0].channel_index == 0);
    REQUIRE(p.channel_plans[0].render_instructions.size() == 1);
    REQUIRE(p.channels.size() == 1);
    REQUIRE(p.render_instructions.size() == 1);
    REQUIRE(p.timeline.size() == 2);
}

TEST_CASE("Per-channel emitter assignment by channel_id", "[planner][multi_channel]") {
    auto result = plan(make_two_channel_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.channel_plans.size() == 2);

    REQUIRE(p.channel_plans[0].render_instructions.size() == 1);
    REQUIRE(p.channel_plans[0].render_instructions[0].emitter_id == "cw0");
    REQUIRE_THAT(p.channel_plans[0].render_instructions[0].start_sec, WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(p.channel_plans[0].render_instructions[0].duration_sec, WithinAbs(5.0, 1e-12));

    REQUIRE(p.channel_plans[1].render_instructions.size() == 1);
    REQUIRE(p.channel_plans[1].render_instructions[0].emitter_id == "cw1");
    REQUIRE_THAT(p.channel_plans[1].render_instructions[0].start_sec, WithinAbs(0.5, 1e-12));
    REQUIRE_THAT(p.channel_plans[1].render_instructions[0].duration_sec, WithinAbs(3.0, 1e-12));
}

TEST_CASE("Per-channel emitter assignment by device+channel pair", "[planner][multi_channel]") {
    auto result = plan(make_two_channel_by_device_channel_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.channel_plans.size() == 2);

    REQUIRE(p.channel_plans[0].render_instructions.size() == 1);
    REQUIRE(p.channel_plans[0].render_instructions[0].emitter_id == "cw0");

    REQUIRE(p.channel_plans[1].render_instructions.size() == 1);
    REQUIRE(p.channel_plans[1].render_instructions[0].emitter_id == "cw1");
}

TEST_CASE("Per-channel event assignment", "[planner][multi_channel]") {
    auto result = plan(make_two_channel_with_events_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.channel_plans.size() == 2);

    auto count_type = [](const ChannelPlan& cp, TimelineEventType t) {
        size_t n = 0;
        for (const auto& e : cp.events) {
            if (e.type == t) ++n;
        }
        return n;
    };

    REQUIRE(count_type(p.channel_plans[0], TimelineEventType::EmitterStart) == 1);
    REQUIRE(count_type(p.channel_plans[0], TimelineEventType::EmitterStop) == 1);
    REQUIRE(count_type(p.channel_plans[1], TimelineEventType::EmitterStart) == 1);
    REQUIRE(count_type(p.channel_plans[1], TimelineEventType::EmitterStop) == 1);

    auto count_non_emitter = [](const ChannelPlan& cp) {
        size_t n = 0;
        for (const auto& e : cp.events) {
            if (e.type != TimelineEventType::EmitterStart && e.type != TimelineEventType::EmitterStop) {
                ++n;
            }
        }
        return n;
    };

    REQUIRE(count_non_emitter(p.channel_plans[0]) == 2);
    REQUIRE(count_non_emitter(p.channel_plans[1]) == 2);
}

TEST_CASE("Per-channel resource estimate", "[planner][multi_channel]") {
    auto result = plan(make_two_channel_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.channel_plans.size() == 2);

    const auto& est0 = p.channel_plans[0].resource_estimate;
    REQUIRE(est0.timing_feasible);
    REQUIRE(est0.peak_memory_bytes > 0);
    REQUIRE(est0.estimated_cpu_load >= 0.0);
    REQUIRE(est0.min_inter_emitter_gap_sec == 0.0);

    const auto& est1 = p.channel_plans[1].resource_estimate;
    REQUIRE(est1.timing_feasible);
    REQUIRE(est1.peak_memory_bytes > 0);
    REQUIRE(est1.estimated_cpu_load >= 0.0);
}
