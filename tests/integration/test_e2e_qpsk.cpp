#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"
#include "archerfish/scenario/plan_io.hpp"
#include "archerfish/hal/stub_device.hpp"
#include "archerfish/runtime/runtime.hpp"

#include <filesystem>

using namespace archerfish::scenario;
using namespace archerfish::runtime;
using namespace archerfish::hal;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

static const char* examples_dir = EXAMPLES_DIR;

TEST_CASE("E2E QPSK: parse qpsk_burst through full pipeline", "[e2e][qpsk]") {
    auto path = std::filesystem::path(examples_dir) / "qpsk_burst.json";
    auto parse_result = parse_scenario(path);
    REQUIRE(parse_result.has_value());

    auto& scenario = *parse_result;
    REQUIRE(scenario.metadata.name == "qpsk_burst");
    REQUIRE(scenario.emitters.size() == 1);
    REQUIRE(scenario.emitters[0].waveform->type == "qpsk");

    REQUIRE(resolve_waveform_refs(scenario).empty());
    REQUIRE(validate(scenario).ok());

    auto plan_result = plan(scenario);
    REQUIRE(plan_result.has_value());

    const auto& p = plan_result.value();
    REQUIRE(p.render_instructions.size() == 1);
    REQUIRE(p.timeline.size() == 2);
    REQUIRE(p.render_instructions[0].waveform.type == "qpsk");
    REQUIRE_THAT(p.render_instructions[0].start_sec, WithinAbs(1.5, 1e-9));
    REQUIRE_THAT(p.render_instructions[0].duration_sec, WithinAbs(0.10, 1e-12));
    REQUIRE_THAT(p.render_instructions[0].sample_rate, WithinAbs(8e6, 1.0));

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

    size_t expected_samples = static_cast<size_t>(8e6 * 0.10);
    REQUIRE(device->total_samples_sent(0) == expected_samples);

    auto metrics = rt.get_metrics();
    REQUIRE(metrics.total_samples_sent == expected_samples);
    REQUIRE(metrics.total_blocks_sent > 0);
    REQUIRE(metrics.actual_duration_sec >= 0.0);
}

TEST_CASE("E2E QPSK: plan JSON contains waveform params", "[e2e][qpsk]") {
    auto path = std::filesystem::path(examples_dir) / "qpsk_burst.json";
    auto parse_result = parse_scenario(path);
    REQUIRE(parse_result.has_value());

    REQUIRE(resolve_waveform_refs(*parse_result).empty());
    REQUIRE(validate(*parse_result).ok());

    auto plan_result = plan(*parse_result);
    REQUIRE(plan_result.has_value());

    auto json = plan_to_json(*plan_result);
    REQUIRE(json.is_object());
    REQUIRE(json["render_instructions"].is_array());
    REQUIRE(json["render_instructions"].size() == 1);

    const auto& wf = json["render_instructions"][0]["waveform"];
    REQUIRE(wf["type"].get<std::string>() == "qpsk");
    REQUIRE(wf["params"].contains("symbol_rate"));
    REQUIRE(wf["params"].contains("samples_per_symbol"));
    REQUIRE(wf["params"].contains("rrc_alpha"));
}
