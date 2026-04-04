#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"
#include "archerfish/scenario/plan_io.hpp"
#include "archerfish/hal/stub_device.hpp"
#include "archerfish/runtime/runtime.hpp"
#include "archerfish/dsp/waveform_type.hpp"

#include <filesystem>

using namespace archerfish::scenario;
using namespace archerfish::runtime;
using namespace archerfish::hal;
using WaveformType = archerfish::dsp::WaveformType;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

static const char* examples_dir = EXAMPLES_DIR;

TEST_CASE("E2E CW: parse future_start_cw through full pipeline", "[e2e][cw]") {
    auto path = std::filesystem::path(examples_dir) / "future_start_cw.json";
    auto parse_result = parse_scenario(path);
    REQUIRE(parse_result.has_value());

    auto& scenario = *parse_result;
    REQUIRE(scenario.metadata.name == "future_start_cw");
    REQUIRE(scenario.devices.size() == 1);
    REQUIRE(scenario.emitters.size() == 1);
    REQUIRE(scenario.emitters[0].waveform->type == WaveformType::CW);

    auto ref_errors = resolve_waveform_refs(scenario);
    REQUIRE(ref_errors.empty());

    auto validation = validate(scenario);
    REQUIRE(validation.ok());

    auto plan_result = plan(scenario);
    REQUIRE(plan_result.has_value());

    const auto& p = plan_result.value();
    REQUIRE(p.render_instructions.size() == 1);
    REQUIRE(p.timeline.size() == 2);
    REQUIRE(p.render_instructions[0].waveform.type == WaveformType::CW);
    REQUIRE_THAT(p.render_instructions[0].start_sec, WithinAbs(2.0, 1e-9));
    REQUIRE_THAT(p.render_instructions[0].duration_sec, WithinAbs(4.0, 1e-9));

    auto plan_json = plan_to_json(p);
    REQUIRE(plan_json.is_object());
    REQUIRE(plan_json.contains("render_instructions"));

    auto device = std::make_shared<StubDevice>();
    RuntimeConfig config;
    config.queue_capacity = 32;
    config.block_size = 4096;
    config.channel = 0;

    Runtime rt(device, config);
    REQUIRE(rt.state() == RuntimeState::Created);

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.state() == RuntimeState::Prepared);

    REQUIRE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Armed);

    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);

    size_t expected_samples = static_cast<size_t>(10e6 * 4.0);
    REQUIRE(device->total_samples_sent(0) > 0);
    REQUIRE(device->total_samples_sent(0) <= expected_samples);

    auto metrics = rt.get_metrics();
    REQUIRE(metrics.total_samples_sent > 0);
    REQUIRE(metrics.total_blocks_sent > 0);
    REQUIRE(metrics.actual_duration_sec > 0.0);
}

TEST_CASE("E2E CW: plan JSON roundtrip preserves data", "[e2e][cw]") {
    auto path = std::filesystem::path(examples_dir) / "future_start_cw.json";
    auto parse_result = parse_scenario(path);
    REQUIRE(parse_result.has_value());

    auto& scenario = *parse_result;
    auto ref_errors2 = resolve_waveform_refs(scenario);
    REQUIRE(ref_errors2.empty());
    REQUIRE(validate(scenario).ok());

    auto plan_result = plan(scenario);
    REQUIRE(plan_result.has_value());

    const auto& p = plan_result.value();
    auto json = plan_to_json(p);

    REQUIRE(json.is_object());
    REQUIRE(json["render_instructions"].is_array());
    REQUIRE(json["render_instructions"].size() == 1);
    REQUIRE(json["timeline"].is_array());
    REQUIRE(json["timeline"].size() == 2);
    REQUIRE(json["channels"].is_array());
    REQUIRE(json["channels"].size() == 1);
}
