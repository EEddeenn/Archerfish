#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/scenario.hpp"
#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;

namespace {

Scenario make_event_scenario() {
    Scenario s;
    s.metadata.name = "event_test";

    DeviceDef dev;
    dev.id = "usrp0";
    dev.channel = 0;
    dev.rf = {2450000000.0, 10000000.0, 20.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 0.0;
    em.duration_sec = 2.0;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    s.emitters.push_back(em);

    ScenarioEvent retune_evt;
    retune_evt.target_device = "usrp0";
    retune_evt.time_sec = 1.0;
    retune_evt.type = "retune";
    retune_evt.payload = nlohmann::json{{"freq_hz", 3500000000.0}};
    s.events.push_back(retune_evt);

    ScenarioEvent gain_evt;
    gain_evt.target_device = "usrp0";
    gain_evt.time_sec = 0.5;
    gain_evt.type = "gain_change";
    gain_evt.payload = nlohmann::json{{"gain_db", 25.0}};
    s.events.push_back(gain_evt);

    return s;
}

} // namespace

TEST_CASE("Timed events: parser reads events section", "[parser][events]") {
    std::string json_str = R"({
        "metadata": {"name": "evt_parse"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [
            {"id": "e1", "device": "d0", "duration_sec": 1.0,
             "waveform": {"type": "cw", "amplitude": 0.3}}
        ],
        "events": [
            {"target_device": "d0", "time_sec": 0.5, "type": "retune", "payload": {"freq_hz": 2e9}},
            {"target_device": "d0", "time_sec": 0.3, "type": "gain_change", "payload": {"gain_db": 15.0}}
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    REQUIRE(result->events.size() == 2);
    REQUIRE(result->events[0].type == "retune");
    REQUIRE(result->events[0].time_sec == 0.5);
    REQUIRE(result->events[1].type == "gain_change");
}

TEST_CASE("Timed events: validator rejects unknown device", "[validator][events]") {
    Scenario s;
    s.metadata.name = "evt_bad";
    DeviceDef dev;
    dev.id = "usrp0";
    dev.rf = {1e9, 1e6, 10.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "e1";
    em.device = "usrp0";
    em.duration_sec = 1.0;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    s.emitters.push_back(em);

    ScenarioEvent evt;
    evt.target_device = "nonexistent";
    evt.time_sec = 0.5;
    evt.type = "retune";
    evt.payload = nlohmann::json{{"freq_hz", 2e9}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V013_EVENT_UNKNOWN_DEVICE") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Timed events: validator rejects invalid event type", "[validator][events]") {
    Scenario s;
    s.metadata.name = "evt_badtype";
    DeviceDef dev;
    dev.id = "usrp0";
    dev.rf = {1e9, 1e6, 10.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "e1";
    em.device = "usrp0";
    em.duration_sec = 1.0;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    s.emitters.push_back(em);

    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.5;
    evt.type = "invalid_type";
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V014_INVALID_EVENT_TYPE") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Timed events: validator rejects retune without freq_hz", "[validator][events]") {
    Scenario s;
    s.metadata.name = "evt_nofreq";
    DeviceDef dev;
    dev.id = "usrp0";
    dev.rf = {1e9, 1e6, 10.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "e1";
    em.device = "usrp0";
    em.duration_sec = 1.0;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    s.emitters.push_back(em);

    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.5;
    evt.type = "retune";
    evt.payload = nlohmann::json{{"something", 123}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V015_RETUNE_MISSING_FREQ") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Timed events: validator rejects gain_change without gain_db", "[validator][events]") {
    Scenario s;
    s.metadata.name = "evt_nogain";
    DeviceDef dev;
    dev.id = "usrp0";
    dev.rf = {1e9, 1e6, 10.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "e1";
    em.device = "usrp0";
    em.duration_sec = 1.0;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    s.emitters.push_back(em);

    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.5;
    evt.type = "gain_change";
    evt.payload = nlohmann::json{{"something", 123}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V016_GAIN_MISSING_DB") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Timed events: planner generates timeline events for retune and gain", "[planner][events]") {
    auto s = make_event_scenario();
    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());
    const auto& p = plan_result.value();

    bool found_freq = false, found_gain = false;
    for (const auto& te : p.timeline) {
        if (te.type == TimelineEventType::FreqChange) found_freq = true;
        if (te.type == TimelineEventType::GainChange) found_gain = true;
    }
    REQUIRE(found_freq);
    REQUIRE(found_gain);
}
