#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "archerfish/scenario/scenario.hpp"
#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

namespace {

Scenario make_mixing_scenario(MixingMode mode_a = MixingMode::Additive,
                              MixingMode mode_b = MixingMode::Additive) {
    Scenario s;
    s.metadata.name = "mix_test";

    DeviceDef dev;
    dev.id = "usrp0";
    dev.channel = 0;
    dev.rf = {2450000000.0, 10000000.0, 20.0};
    s.devices.push_back(dev);

    EmitterDef e1;
    e1.id = "cw1";
    e1.device = "usrp0";
    e1.channel = 0;
    e1.start_after_sec = 0.0;
    e1.duration_sec = 1.0;
    e1.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    e1.mixing = mode_a;
    s.emitters.push_back(e1);

    EmitterDef e2;
    e2.id = "cw2";
    e2.device = "usrp0";
    e2.channel = 0;
    e2.start_after_sec = 0.0;
    e2.duration_sec = 1.0;
    e2.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.4}}};
    e2.mixing = mode_b;
    s.emitters.push_back(e2);

    return s;
}

} // namespace

TEST_CASE("Additive mixing: overlapping emitters with mixing=none rejected", "[scheduler][mixing]") {
    auto s = make_mixing_scenario(MixingMode::None, MixingMode::None);
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    bool found_overlap = false;
    for (const auto& e : result.errors) {
        if (e.code == "V002_OVERLAPPING_EMITTERS") found_overlap = true;
    }
    REQUIRE(found_overlap);
}

TEST_CASE("Additive mixing: overlapping emitters with mixing=additive allowed", "[scheduler][mixing]") {
    auto s = make_mixing_scenario(MixingMode::Additive, MixingMode::Additive);
    auto result = validate(s);
    REQUIRE(result.ok());
}

TEST_CASE("Additive mixing: mixed modes (one none, one additive) rejected", "[scheduler][mixing]") {
    auto s = make_mixing_scenario(MixingMode::None, MixingMode::Additive);
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
}

TEST_CASE("Additive mixing: headroom warning when peak sum > 1.0", "[scheduler][mixing]") {
    Scenario s;
    s.metadata.name = "headroom_test";

    DeviceDef dev;
    dev.id = "usrp0";
    dev.channel = 0;
    dev.rf = {2450000000.0, 10000000.0, 20.0};
    s.devices.push_back(dev);

    EmitterDef e1;
    e1.id = "cw1";
    e1.device = "usrp0";
    e1.channel = 0;
    e1.start_after_sec = 0.0;
    e1.duration_sec = 1.0;
    e1.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.7}}};
    e1.mixing = MixingMode::Additive;
    s.emitters.push_back(e1);

    EmitterDef e2;
    e2.id = "cw2";
    e2.device = "usrp0";
    e2.channel = 0;
    e2.start_after_sec = 0.0;
    e2.duration_sec = 1.0;
    e2.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.6}}};
    e2.mixing = MixingMode::Additive;
    s.emitters.push_back(e2);

    auto result = validate(s);
    REQUIRE(result.ok());
    bool found_headroom = false;
    for (const auto& w : result.warnings) {
        if (w.code == "W_MIX_HEADROOM") found_headroom = true;
    }
    REQUIRE(found_headroom);
}

TEST_CASE("Additive mixing: planner creates MixGroup for overlapping additive emitters", "[scheduler][mixing]") {
    auto s = make_mixing_scenario();
    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());
    const auto& p = plan_result.value();

    REQUIRE(p.mix_groups.size() == 1);
    REQUIRE(p.mix_groups[0].device_id == "usrp0");
    REQUIRE(p.mix_groups[0].channel == 0);
    REQUIRE(p.mix_groups[0].emitter_ids.size() == 2);
    REQUIRE_THAT(p.mix_groups[0].estimated_peak_sum, WithinAbs(0.7, 1e-9));
}

TEST_CASE("Additive mixing: non-overlapping emitters no mix group", "[scheduler][mixing]") {
    Scenario s;
    s.metadata.name = "nonoverlap_test";

    DeviceDef dev;
    dev.id = "usrp0";
    dev.channel = 0;
    dev.rf = {2450000000.0, 10000000.0, 20.0};
    s.devices.push_back(dev);

    EmitterDef e1;
    e1.id = "cw1";
    e1.device = "usrp0";
    e1.channel = 0;
    e1.start_after_sec = 0.0;
    e1.duration_sec = 1.0;
    e1.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    e1.mixing = MixingMode::Additive;
    s.emitters.push_back(e1);

    EmitterDef e2;
    e2.id = "cw2";
    e2.device = "usrp0";
    e2.channel = 0;
    e2.start_after_sec = 2.0;
    e2.duration_sec = 1.0;
    e2.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.4}}};
    e2.mixing = MixingMode::Additive;
    s.emitters.push_back(e2);

    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());
    REQUIRE(plan_result->mix_groups.empty());
}

TEST_CASE("Additive mixing: parser reads mixing field", "[parser][mixing]") {
    std::string json_str = R"({
        "metadata": {"name": "mix_parse"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [
            {"id": "e1", "device": "d0", "duration_sec": 1.0, "mixing": "additive",
             "waveform": {"type": "cw", "amplitude": 0.3}},
            {"id": "e2", "device": "d0", "duration_sec": 1.0, "mixing": "none",
             "waveform": {"type": "cw", "amplitude": 0.3}}
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    REQUIRE(result->emitters[0].mixing == MixingMode::Additive);
    REQUIRE(result->emitters[1].mixing == MixingMode::None);
}
