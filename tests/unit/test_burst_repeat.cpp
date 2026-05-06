#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/scenario.hpp"
#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"

#include <limits>

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

namespace {

Scenario make_repeat_scenario(int count, double interval) {
    Scenario s;
    s.metadata.name = "repeat_test";

    DeviceDef dev;
    dev.id = "usrp0";
    dev.channel = 0;
    dev.rf = {2450000000.0, 10000000.0, 20.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "burst";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 0.0;
    em.duration_sec = 0.1;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    em.repeat = RepeatSpec{count, interval};
    s.emitters.push_back(em);

    return s;
}

} // namespace

TEST_CASE("Burst repeat: planner unrolls N repetitions", "[scheduler][repeat]") {
    auto s = make_repeat_scenario(5, 0.5);
    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());
    const auto& p = plan_result.value();
    REQUIRE(p.render_instructions.size() == 5);

    for (int i = 0; i < 5; ++i) {
        REQUIRE_THAT(p.render_instructions[i].start_sec, WithinAbs(i * 0.5, 1e-9));
        REQUIRE_THAT(p.render_instructions[i].duration_sec, WithinAbs(0.1, 1e-9));
    }
}

TEST_CASE("Burst repeat: single repetition (count=1) produces one instruction", "[scheduler][repeat]") {
    auto s = make_repeat_scenario(1, 0.0);
    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->render_instructions.size() == 1);
    REQUIRE(plan_result->render_instructions[0].emitter_id == "burst");
}

TEST_CASE("Burst repeat: validator rejects count < 1", "[validator][repeat]") {
    Scenario s;
    s.metadata.name = "bad_repeat";
    DeviceDef dev;
    dev.id = "usrp0";
    dev.rf = {1e9, 1e6, 10.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "e1";
    em.device = "usrp0";
    em.duration_sec = 1.0;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    em.repeat = RepeatSpec{0, 0.5};
    s.emitters.push_back(em);

    auto result = validate(s);
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V017_INVALID_REPEAT_COUNT") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Burst repeat: validator rejects excessive repeat count", "[validator][repeat]") {
    auto s = make_repeat_scenario(1025, 0.5);

    auto result = validate(s);
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V017_INVALID_REPEAT_COUNT") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Burst repeat: planner rejects excessive repeat count", "[scheduler][repeat]") {
    auto s = make_repeat_scenario(1025, 0.5);

    auto plan_result = plan(s);
    REQUIRE_FALSE(plan_result.has_value());
    bool found = false;
    for (const auto& e : plan_result.error()) {
        if (e.code == "E_PLAN_INVALID_REPEAT") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Burst repeat: validator rejects interval <= 0 when count > 1", "[validator][repeat]") {
    Scenario s;
    s.metadata.name = "bad_interval";
    DeviceDef dev;
    dev.id = "usrp0";
    dev.rf = {1e9, 1e6, 10.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "e1";
    em.device = "usrp0";
    em.duration_sec = 1.0;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    em.repeat = RepeatSpec{3, 0.0};
    s.emitters.push_back(em);

    auto result = validate(s);
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V018_INVALID_REPEAT_INTERVAL") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Burst repeat: validator rejects non-finite interval", "[validator][repeat]") {
    auto s = make_repeat_scenario(1, std::numeric_limits<double>::quiet_NaN());

    auto result = validate(s);
    bool found = false;
    for (const auto& e : result.errors) {
        if (e.code == "V018_INVALID_REPEAT_INTERVAL") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Burst repeat: parser reads repeat section", "[parser][repeat]") {
    std::string json_str = R"({
        "metadata": {"name": "repeat_parse"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [
            {"id": "e1", "device": "d0", "duration_sec": 0.1,
             "waveform": {"type": "cw", "amplitude": 0.3},
             "repeat": {"count": 10, "interval_sec": 0.2}}
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    REQUIRE(result->emitters[0].repeat.has_value());
    REQUIRE(result->emitters[0].repeat->count == 10);
    REQUIRE_THAT(result->emitters[0].repeat->interval_sec, WithinAbs(0.2, 1e-9));
}
