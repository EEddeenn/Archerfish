#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/impairments/impairment_chain.hpp"
#include "archerfish/impairments/dc_offset.hpp"
#include "archerfish/impairments/awgn.hpp"
#include "archerfish/scenario/scenario.hpp"
#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::scenario;
using namespace archerfish::common;

namespace {

Scenario make_impairment_change_scenario() {
    Scenario s;
    s.metadata.name = "impairment_change_test";

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
    ImpairmentSettings imp;
    imp.dc_offset_i = 0.5;
    imp.dc_offset_q = -0.3;
    em.impairments = imp;
    s.emitters.push_back(em);

    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 1.0;
    evt.type = "impairment_change";
    evt.payload = nlohmann::json{
        {"emitter_id", "cw1"},
        {"impairment", "dc_offset"},
        {"enabled", false}};
    s.events.push_back(evt);

    return s;
}

} // namespace

TEST_CASE("Parser reads impairment_change event", "[parser][impairment_change]") {
    std::string json_str = R"({
        "metadata": {"name": "ic_parse"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [
            {"id": "e1", "device": "d0", "duration_sec": 1.0,
             "waveform": {"type": "cw", "amplitude": 0.3},
             "impairments": {"dc_offset_i": 0.1}}
        ],
        "events": [
            {"target_device": "d0", "time_sec": 0.5, "type": "impairment_change",
             "payload": {"emitter_id": "e1", "impairment": "dc_offset", "enabled": false}}
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    REQUIRE(result->events.size() == 1);
    REQUIRE(result->events[0].type == "impairment_change");
    REQUIRE(result->events[0].time_sec == 0.5);
    REQUIRE(result->events[0].payload["emitter_id"] == "e1");
    REQUIRE(result->events[0].payload["impairment"] == "dc_offset");
    REQUIRE(result->events[0].payload["enabled"] == false);
}

TEST_CASE("Validator accepts valid impairment_change event", "[validator][impairment_change]") {
    auto s = make_impairment_change_scenario();
    auto result = validate(s);
    REQUIRE(result.ok());
}

TEST_CASE("Validator rejects impairment_change without emitter_id", "[validator][impairment_change]") {
    Scenario s;
    s.metadata.name = "ic_no_emitter";
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
    evt.type = "impairment_change";
    evt.payload = nlohmann::json{{"impairment", "dc_offset"}, {"enabled", false}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V020_IMPAIRMENT_CHANGE_MISSING_EMITTER") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Validator rejects impairment_change without impairment name", "[validator][impairment_change]") {
    Scenario s;
    s.metadata.name = "ic_no_name";
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
    evt.type = "impairment_change";
    evt.payload = nlohmann::json{{"emitter_id", "e1"}, {"enabled", true}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V020_IMPAIRMENT_CHANGE_MISSING_IMPAIRMENT") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Planner generates ImpairmentChange timeline event", "[planner][impairment_change]") {
    auto s = make_impairment_change_scenario();
    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());
    const auto& p = plan_result.value();

    bool found = false;
    for (const auto& te : p.timeline) {
        if (te.type == TimelineEventType::ImpairmentChange) {
            found = true;
            REQUIRE(te.time_sec == 1.0);
            REQUIRE(te.payload["emitter_id"] == "cw1");
            REQUIRE(te.payload["impairment"] == "dc_offset");
            REQUIRE(te.payload["enabled"] == false);
        }
    }
    REQUIRE(found);
}

TEST_CASE("ImpairmentChain toggle via set_enabled", "[impairment_chain][impairment_change]") {
    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(1.0, 2.0));
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(0.5, -0.5));

    REQUIRE(chain.size() == 2);
    REQUIRE(chain.at(0).name() == "dc_offset");
    REQUIRE(chain.at(0).enabled());

    std::vector<std::complex<float>> data1 = {{0.0f, 0.0f}};
    chain.apply(data1.data(), data1.size());
    REQUIRE_THAT(data1[0].real(), WithinAbs(1.5, 1e-5));
    REQUIRE_THAT(data1[0].imag(), WithinAbs(1.5, 1e-5));

    std::string target_name = "dc_offset";
    bool enabled = false;
    for (size_t i = 0; i < chain.size(); ++i) {
        if (chain.at(i).name() == target_name) {
            chain.set_enabled(i, enabled);
            break;
        }
    }

    REQUIRE_FALSE(chain.at(0).enabled());
    REQUIRE(chain.at(1).enabled());

    std::vector<std::complex<float>> data2 = {{0.0f, 0.0f}};
    chain.apply(data2.data(), data2.size());
    REQUIRE_THAT(data2[0].real(), WithinAbs(0.5, 1e-5));
    REQUIRE_THAT(data2[0].imag(), WithinAbs(-0.5, 1e-5));
}

TEST_CASE("ImpairmentChain re-enable impairment", "[impairment_chain][impairment_change]") {
    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(0.7, 0.0));

    chain.set_enabled(0, false);
    std::vector<std::complex<float>> data_off = {{1.0f, 1.0f}};
    chain.apply(data_off.data(), data_off.size());
    REQUIRE_THAT(data_off[0].real(), WithinAbs(1.0, 1e-5));
    REQUIRE_THAT(data_off[0].imag(), WithinAbs(1.0, 1e-5));

    chain.set_enabled(0, true);
    std::vector<std::complex<float>> data_on = {{1.0f, 1.0f}};
    chain.apply(data_on.data(), data_on.size());
    REQUIRE_THAT(data_on[0].real(), WithinAbs(1.7, 1e-5));
    REQUIRE_THAT(data_on[0].imag(), WithinAbs(1.0, 1e-5));
}

TEST_CASE("Multiple impairment_change events in scenario", "[parser][planner][impairment_change]") {
    std::string json_str = R"({
        "metadata": {"name": "multi_ic"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [
            {"id": "e1", "device": "d0", "duration_sec": 2.0,
             "waveform": {"type": "cw", "amplitude": 0.3},
             "impairments": {"dc_offset_i": 0.1, "awgn_power": 0.01}}
        ],
        "events": [
            {"target_device": "d0", "time_sec": 0.5, "type": "impairment_change",
             "payload": {"emitter_id": "e1", "impairment": "dc_offset", "enabled": false}},
            {"target_device": "d0", "time_sec": 1.0, "type": "impairment_change",
             "payload": {"emitter_id": "e1", "impairment": "awgn", "enabled": false}},
            {"target_device": "d0", "time_sec": 1.5, "type": "impairment_change",
             "payload": {"emitter_id": "e1", "impairment": "dc_offset", "enabled": true}}
        ]
    })";

    auto parsed = parse_scenario_json(json_str);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->events.size() == 3);

    auto validated = validate(*parsed);
    REQUIRE(validated.ok());

    auto plan_result = plan(*parsed);
    REQUIRE(plan_result.has_value());

    int impairment_count = 0;
    for (const auto& te : plan_result->timeline) {
        if (te.type == TimelineEventType::ImpairmentChange) {
            impairment_count++;
        }
    }
    REQUIRE(impairment_count == 3);
}
