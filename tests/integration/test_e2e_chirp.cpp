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

TEST_CASE("E2E Chirp: parse chirp_burst through full pipeline", "[e2e][chirp]") {
    auto path = std::filesystem::path(examples_dir) / "chirp_burst.json";
    auto parse_result = parse_scenario(path);
    REQUIRE(parse_result.has_value());

    auto& scenario = *parse_result;
    REQUIRE(scenario.metadata.name == "chirp_burst");
    REQUIRE(scenario.emitters.size() == 1);
    REQUIRE(scenario.emitters[0].waveform->type == WaveformType::Chirp);

    REQUIRE(resolve_waveform_refs(scenario).empty());
    REQUIRE(validate(scenario).ok());

    auto plan_result = plan(scenario);
    REQUIRE(plan_result.has_value());

    const auto& p = plan_result.value();
    REQUIRE(p.render_instructions.size() == 1);
    REQUIRE(p.timeline.size() == 2);
    REQUIRE(p.render_instructions[0].waveform.type == WaveformType::Chirp);
    REQUIRE_THAT(p.render_instructions[0].start_sec, WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(p.render_instructions[0].duration_sec, WithinAbs(0.02, 1e-12));
    REQUIRE_THAT(p.render_instructions[0].sample_rate, WithinAbs(20e6, 1.0));

    auto device = std::make_shared<StubDevice>();
    RuntimeConfig config;
    config.queue_capacity = 32;
    config.block_size = 4096;
    config.channel = 0;

    Runtime rt(device, config);
    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);

    size_t expected_samples = static_cast<size_t>(20e6 * 0.02);
    REQUIRE(device->total_samples_sent(0) == expected_samples);

    auto metrics = rt.get_metrics();
    REQUIRE(metrics.total_samples_sent == expected_samples);
    REQUIRE(metrics.total_blocks_sent > 0);
}

TEST_CASE("E2E Chirp: verifies short duration sample count", "[e2e][chirp]") {
    auto path = std::filesystem::path(examples_dir) / "chirp_burst.json";
    auto parse_result = parse_scenario(path);
    REQUIRE(parse_result.has_value());

    REQUIRE(resolve_waveform_refs(*parse_result).empty());
    REQUIRE(validate(*parse_result).ok());

    auto plan_result = plan(*parse_result);
    REQUIRE(plan_result.has_value());

    const auto& instr = plan_result->render_instructions[0];
    double sample_rate = 20e6;
    double duration = 0.02;
    size_t expected = static_cast<size_t>(sample_rate * duration);
    REQUIRE(expected == 400000);
    REQUIRE_THAT(instr.sample_rate, WithinAbs(sample_rate, 1.0));
    REQUIRE_THAT(instr.duration_sec, WithinAbs(duration, 1e-12));
}
