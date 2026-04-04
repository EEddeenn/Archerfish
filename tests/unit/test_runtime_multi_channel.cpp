#include <catch2/catch_test_macros.hpp>

#include <archerfish/runtime/runtime.hpp>
#include <archerfish/hal/stub_device.hpp>
#include <archerfish/scenario/planner.hpp>

using namespace archerfish::runtime;
using namespace archerfish::hal;

static archerfish::scenario::Plan make_multi_channel_plan() {
    archerfish::scenario::Scenario s;
    s.metadata.name = "mc_runtime";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    archerfish::scenario::EmitterDef em;
    em.id = "em0";
    em.device = "stub0";
    em.channel = 0;
    em.start_after_sec = 0.0;
    em.duration_sec = 0.005;
    em.waveform = archerfish::scenario::WaveformDef{
        std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}};
    s.emitters.push_back(em);
    auto result = archerfish::scenario::plan(s);
    REQUIRE(result.has_value());
    return *result;
}

static archerfish::scenario::Plan make_two_device_plan() {
    archerfish::scenario::Scenario s;
    s.metadata.name = "mc_two_dev";
    s.devices.push_back({"dev0", 0, {1e9, 1e6, 10.0}});
    s.devices.push_back({"dev1", 0, {2e9, 2e6, 15.0}});

    archerfish::scenario::EmitterDef em0;
    em0.id = "em0";
    em0.device = "dev0";
    em0.channel = 0;
    em0.start_after_sec = 0.0;
    em0.duration_sec = 0.005;
    em0.waveform = archerfish::scenario::WaveformDef{
        std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}};

    archerfish::scenario::EmitterDef em1;
    em1.id = "em1";
    em1.device = "dev1";
    em1.channel = 0;
    em1.start_after_sec = 0.0;
    em1.duration_sec = 0.005;
    em1.waveform = archerfish::scenario::WaveformDef{
        std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.2}}};

    s.emitters.push_back(em0);
    s.emitters.push_back(em1);
    auto result = archerfish::scenario::plan(s);
    REQUIRE(result.has_value());
    return *result;
}

TEST_CASE("Multi-channel runtime prepares successfully", "[runtime][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);
    auto p = make_multi_channel_plan();
    REQUIRE(rt.prepare(p));
}

TEST_CASE("Multi-channel runtime arms successfully", "[runtime][multi_channel][arms]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);
    auto p = make_multi_channel_plan();
    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
}

TEST_CASE("Two-device plan has channel plans", "[runtime][multi_channel]") {
    auto p = make_two_device_plan();
    REQUIRE_FALSE(p.channel_plans.empty());
}

TEST_CASE("Two-device plan has render instructions per channel", "[runtime][multi_channel]") {
    auto p = make_two_device_plan();
    size_t total = 0;
    for ([[maybe_unused]] const auto& ri : p.render_instructions) {
        total++;
    }
    REQUIRE(total == 2);
}

TEST_CASE("Multi-channel plan has channel plans", "[runtime][multi_channel][plan]") {
    auto p = make_multi_channel_plan();
    REQUIRE_FALSE(p.channel_plans.empty());
}

TEST_CASE("Multi-channel plan per-channel render instructions", "[runtime][multi_channel][plan]") {
    auto p = make_multi_channel_plan();
    size_t total = 0;
    for (const auto& cp : p.channel_plans) {
        total += cp.render_instructions.size();
    }
    REQUIRE(total >= 1);
}
