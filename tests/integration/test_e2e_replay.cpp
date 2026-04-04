#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"
#include "archerfish/hal/stub_device.hpp"
#include "archerfish/runtime/runtime.hpp"

#include <complex>
#include <filesystem>
#include "archerfish/dsp/waveform_type.hpp"
#include <fstream>
#include <vector>

using namespace archerfish::scenario;
using namespace archerfish::runtime;
using namespace archerfish::hal;
using WaveformType = archerfish::dsp::WaveformType;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

TEST_CASE("E2E Replay: file waveform plays known samples", "[e2e][replay]") {
    auto temp_dir = std::filesystem::temp_directory_path() / "archerfish_e2e_replay";
    std::filesystem::create_directories(temp_dir);

    auto cf32_path = temp_dir / "test_samples.cf32";

    constexpr size_t num_samples = 4096;
    std::vector<std::complex<float>> known_samples(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(num_samples);
        known_samples[i] = {t, -t};
    }

    {
        std::ofstream out(cf32_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(known_samples.data()),
                  static_cast<std::streamsize>(num_samples * sizeof(std::complex<float>)));
    }

    std::string file_path_str = cf32_path.string();
    std::string scenario_json = std::string(R"({
        "metadata": { "name": "replay_test" },
        "devices": [{
            "id": "usrp0",
            "channel": 0,
            "rf": { "freq_hz": 1000000000.0, "rate_sps": 1000000.0, "gain_db": 10.0 }
        }],
        "emitters": [{
            "id": "file_em",
            "device": "usrp0",
            "channel": 0,
            "start_after_sec": 0.0,
            "duration_sec": 0.004096,
            "waveform": {
                "type": "file",
                "path": ")") + file_path_str + std::string(R"("
            }
        }]
    })");

    auto parse_result = parse_scenario_json(scenario_json);
    REQUIRE(parse_result.has_value());

    auto& scenario = *parse_result;
    REQUIRE(scenario.emitters.size() == 1);
    REQUIRE(scenario.emitters[0].waveform->type == WaveformType::File);

    REQUIRE(resolve_waveform_refs(scenario).empty());
    auto validation = validate(scenario);
    REQUIRE(validation.ok());

    auto plan_result = plan(scenario);
    REQUIRE(plan_result.has_value());

    const auto& p = plan_result.value();
    REQUIRE(p.render_instructions.size() == 1);
    REQUIRE(p.render_instructions[0].waveform.type == WaveformType::File);

    auto device = std::make_shared<StubDevice>();
    RuntimeConfig config;
    config.queue_capacity = 32;
    config.block_size = 1024;
    config.channel = 0;

    Runtime rt(device, config);
    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);

    size_t expected_samples = static_cast<size_t>(1e6 * 0.004096);
    REQUIRE(device->total_samples_sent(0) == expected_samples);

    auto metrics = rt.get_metrics();
    REQUIRE(metrics.total_samples_sent == expected_samples);
    REQUIRE(metrics.total_blocks_sent > 0);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("E2E Replay: nonexistent file produces zero samples", "[e2e][replay]") {
    auto temp_dir = std::filesystem::temp_directory_path() / "archerfish_e2e_replay_missing";
    std::filesystem::create_directories(temp_dir);
    auto bad_path = temp_dir / "nonexistent.cf32";
    std::string bad_path_str = bad_path.string();

    std::string scenario_json = std::string(R"({
        "metadata": { "name": "replay_missing" },
        "devices": [{
            "id": "usrp0",
            "channel": 0,
            "rf": { "freq_hz": 1000000000.0, "rate_sps": 1000000.0, "gain_db": 10.0 }
        }],
        "emitters": [{
            "id": "file_em",
            "device": "usrp0",
            "channel": 0,
            "start_after_sec": 0.0,
            "duration_sec": 0.001,
            "waveform": {
                "type": "file",
                "path": ")") + bad_path_str + std::string(R"("
            }
        }]
    })");

    auto parse_result = parse_scenario_json(scenario_json);
    REQUIRE(parse_result.has_value());

    auto& scenario = *parse_result;
    REQUIRE(resolve_waveform_refs(scenario).empty());
    auto validation = validate(scenario);
    REQUIRE(validation.ok());

    auto plan_result = plan(scenario);
    REQUIRE(plan_result.has_value());

    auto device = std::make_shared<StubDevice>();
    RuntimeConfig config;
    config.queue_capacity = 32;
    config.block_size = 1024;
    config.channel = 0;

    Runtime rt(device, config);
    REQUIRE(rt.prepare(*plan_result));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);

    REQUIRE(device->total_samples_sent(0) == 0);

    std::filesystem::remove_all(temp_dir);
}
