#include <catch2/catch_test_macros.hpp>

#include "archerfish/runtime/runtime.hpp"
#include "archerfish/runtime/state.hpp"
#include "archerfish/hal/stub_device.hpp"
#include "archerfish/scenario/planner.hpp"

using namespace archerfish::runtime;
using namespace archerfish::hal;

TEST_CASE("Runtime initial state is Created", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);
    REQUIRE(rt.state() == RuntimeState::Created);
}

TEST_CASE("Runtime prepare transitions to Prepared", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);

    archerfish::scenario::Scenario s;
    s.metadata.name = "lc_test";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 0.01,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.5}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());
    REQUIRE(rt.prepare(*plan_result));
    REQUIRE(rt.state() == RuntimeState::Prepared);
}

TEST_CASE("Runtime arm transitions Prepared to Armed", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);

    archerfish::scenario::Scenario s;
    s.metadata.name = "arm_test";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 0.01,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.5}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());
    REQUIRE(rt.prepare(*plan_result));
    REQUIRE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Armed);
}

TEST_CASE("Runtime full lifecycle Created -> Completed", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Scenario s;
    s.metadata.name = "full_lc";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 0.005,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());

    REQUIRE(rt.prepare(*plan_result));
    REQUIRE(rt.state() == RuntimeState::Prepared);

    REQUIRE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Armed);

    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);
}

TEST_CASE("Runtime abort transitions to Aborted", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);

    archerfish::scenario::Scenario s;
    s.metadata.name = "abort_test";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 10.0,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());
    REQUIRE(rt.prepare(*plan_result));
    REQUIRE(rt.arm());

    rt.abort();
    REQUIRE(rt.state() == RuntimeState::Aborted);
}
