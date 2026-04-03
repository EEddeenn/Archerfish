#include <catch2/catch_test_macros.hpp>

#include "archerfish/hal/stub_device.hpp"
#include "archerfish/runtime/runtime.hpp"
#include "archerfish/scenario/plan.hpp"

using namespace archerfish::runtime;
using namespace archerfish::hal;
using namespace archerfish::scenario;

TEST_CASE("Runtime full pipeline with single CW emitter", "[integration][runtime]") {
    auto device = std::make_shared<StubDevice>();
    RuntimeConfig config;
    config.queue_capacity = 32;
    config.block_size = 4096;
    config.channel = 0;

    Runtime rt(device, config);
    REQUIRE(rt.state() == RuntimeState::Created);

    Plan plan;
    plan.channels.push_back(ChannelBinding{
        "stub0", 0, RfSettings{1e9, 1e6, 20.0, 800e3, "TX/RX"}
    });

    RenderInstruction instr;
    instr.emitter_id = "cw1";
    instr.waveform = WaveformDef{std::nullopt, "cw", nlohmann::json{{"frequency_hz", 1000.0}, {"amplitude", 0.5}}};
    instr.start_sec = 0.0;
    instr.duration_sec = 0.01;
    instr.sample_rate = 1e6;
    plan.render_instructions.push_back(instr);
    plan.estimated_duration_sec = 0.01;

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.state() == RuntimeState::Prepared);

    REQUIRE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Armed);

    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);

    size_t expected_samples = static_cast<size_t>(1e6 * 0.01);
    REQUIRE(device->total_samples_sent(0) == expected_samples);

    auto metrics = rt.get_metrics();
    REQUIRE(metrics.total_samples_sent == expected_samples);
    REQUIRE(metrics.total_blocks_sent > 0);
    REQUIRE(metrics.actual_duration_sec >= 0.0);
}

TEST_CASE("Runtime state transitions are correct", "[integration][runtime]") {
    auto device = std::make_shared<StubDevice>();
    Runtime rt(device);

    REQUIRE(rt.state() == RuntimeState::Created);

    Plan plan;
    plan.channels.push_back(ChannelBinding{"stub0", 0, RfSettings{1e9, 1e6, 20.0}});

    RenderInstruction instr;
    instr.emitter_id = "cw1";
    instr.waveform = WaveformDef{std::nullopt, "cw", nlohmann::json{{"frequency_hz", 1000.0}, {"amplitude", 0.5}}};
    instr.duration_sec = 0.001;
    instr.sample_rate = 1e6;
    plan.render_instructions.push_back(instr);

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.state() == RuntimeState::Prepared);

    REQUIRE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Armed);

    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);
}

TEST_CASE("Runtime configures device RF settings from plan", "[integration][runtime]") {
    auto device = std::make_shared<StubDevice>();
    Runtime rt(device);

    Plan plan;
    plan.channels.push_back(ChannelBinding{
        "stub0", 0, RfSettings{2.45e9, 10e6, 25.0, 8e6, "TX/RX"}
    });

    RenderInstruction instr;
    instr.emitter_id = "cw1";
    instr.waveform = WaveformDef{std::nullopt, "cw", nlohmann::json{{"frequency_hz", 1000.0}, {"amplitude", 0.5}}};
    instr.duration_sec = 0.001;
    instr.sample_rate = 10e6;
    plan.render_instructions.push_back(instr);

    REQUIRE(rt.prepare(plan));

    const auto& history = device->call_history();
    bool found_freq = false;
    bool found_rate = false;
    bool found_gain = false;
    for (const auto& call : history) {
        if (call.method == "set_center_freq" && call.value == 2.45e9) found_freq = true;
        if (call.method == "set_sample_rate" && call.value == 10e6) found_rate = true;
        if (call.method == "set_gain" && call.value == 25.0) found_gain = true;
    }
    REQUIRE(found_freq);
    REQUIRE(found_rate);
    REQUIRE(found_gain);
}

TEST_CASE("Runtime abort transitions correctly", "[integration][runtime]") {
    auto device = std::make_shared<StubDevice>();
    Runtime rt(device);

    Plan plan;
    plan.channels.push_back(ChannelBinding{"stub0", 0, RfSettings{1e9, 1e6, 20.0}});

    RenderInstruction instr;
    instr.emitter_id = "cw1";
    instr.waveform = WaveformDef{std::nullopt, "cw", nlohmann::json{{"frequency_hz", 1000.0}, {"amplitude", 0.5}}};
    instr.duration_sec = 0.001;
    instr.sample_rate = 1e6;
    plan.render_instructions.push_back(instr);

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());

    rt.abort();
    REQUIRE(rt.state() == RuntimeState::Aborted);
}
