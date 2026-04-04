#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/scenario.hpp"
#include "archerfish/scenario/planner.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

namespace {

Scenario make_resource_scenario() {
    Scenario s;
    s.metadata.name = "resource_test";

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
    s.emitters.push_back(e1);

    EmitterDef e2;
    e2.id = "chirp1";
    e2.device = "usrp0";
    e2.channel = 0;
    e2.start_after_sec = 1.5;
    e2.duration_sec = 1.0;
    e2.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::Chirp, {{"amplitude", 0.4}}};
    s.emitters.push_back(e2);

    return s;
}

} // namespace

TEST_CASE("Resource estimate: CPU load computed from emitter specs", "[planner][diagnostics]") {
    auto s = make_resource_scenario();
    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());

    const auto& est = plan_result->resource_estimate;
    REQUIRE(est.estimated_cpu_load >= 0.0);
    REQUIRE(est.peak_memory_bytes > 0);
    REQUIRE(est.min_inter_emitter_gap_sec >= 0.0);
}

TEST_CASE("Resource estimate: gap between emitters computed", "[planner][diagnostics]") {
    auto s = make_resource_scenario();
    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());

    const auto& est = plan_result->resource_estimate;
    REQUIRE_THAT(est.min_inter_emitter_gap_sec, WithinAbs(0.5, 0.01));
}

TEST_CASE("Resource estimate: timing feasible with reasonable gaps", "[planner][diagnostics]") {
    auto s = make_resource_scenario();
    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());

    const auto& est = plan_result->resource_estimate;
    REQUIRE(est.timing_feasible);
}

TEST_CASE("Resource estimate: tight gap produces warning", "[planner][diagnostics]") {
    Scenario s;
    s.metadata.name = "tight_gap";

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
    s.emitters.push_back(e1);

    EmitterDef e2;
    e2.id = "cw2";
    e2.device = "usrp0";
    e2.channel = 0;
    e2.start_after_sec = 1.00005;
    e2.duration_sec = 1.0;
    e2.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    s.emitters.push_back(e2);

    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());

    const auto& est = plan_result->resource_estimate;
    REQUIRE(est.min_inter_emitter_gap_sec < 100e-6);
}

TEST_CASE("Resource estimate: single emitter has zero gap", "[planner][diagnostics]") {
    Scenario s;
    s.metadata.name = "single";

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
    s.emitters.push_back(e1);

    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());

    const auto& est = plan_result->resource_estimate;
    REQUIRE(est.min_inter_emitter_gap_sec == 0.0);
    REQUIRE(est.timing_feasible);
}
