#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/hal/stub_device.hpp"
#include "archerfish/runtime/runtime.hpp"
#include "archerfish/scenario/plan.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::runtime;
using namespace archerfish::hal;
using namespace archerfish::scenario;
using WaveformType = archerfish::dsp::WaveformType;
using Catch::Matchers::WithinAbs;

TEST_CASE("Multi-channel 2-channel CW run produces per-channel metrics", "[integration][multi_channel]") {
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
    plan.channels.push_back(ChannelBinding{
        "stub0", 1, RfSettings{2.4e9, 1e6, 25.0, 800e3, "TX/RX"}
    });

    RenderInstruction instr0;
    instr0.emitter_id = "cw_ch0";
    instr0.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"frequency_hz", 1000.0}, {"amplitude", 0.5}}};
    instr0.start_sec = 0.0;
    instr0.duration_sec = 0.01;
    instr0.sample_rate = 1e6;
    plan.render_instructions.push_back(instr0);

    RenderInstruction instr1;
    instr1.emitter_id = "cw_ch1";
    instr1.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"frequency_hz", 2000.0}, {"amplitude", 0.3}}};
    instr1.start_sec = 0.0;
    instr1.duration_sec = 0.01;
    instr1.sample_rate = 1e6;
    plan.render_instructions.push_back(instr1);

    plan.estimated_duration_sec = 0.01;

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.state() == RuntimeState::Prepared);

    REQUIRE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Armed);

    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);

    auto metrics = rt.get_metrics();
    REQUIRE(metrics.total_samples_sent > 0);
    REQUIRE(metrics.total_blocks_sent > 0);
    REQUIRE(metrics.actual_duration_sec > 0.0);

    REQUIRE(metrics.per_channel.size() == 2);
    REQUIRE(metrics.per_channel[0].channel_index == 0);
    REQUIRE(metrics.per_channel[1].channel_index == 1);

    size_t expected_samples = static_cast<size_t>(1e6 * 0.01);
    REQUIRE(metrics.per_channel[0].samples_sent == expected_samples);
    REQUIRE(metrics.per_channel[0].blocks_sent > 0);
    REQUIRE(metrics.per_channel[1].samples_sent == expected_samples);
    REQUIRE(metrics.per_channel[1].blocks_sent > 0);

    REQUIRE(device->total_samples_sent(0) > 0);
    REQUIRE(device->total_samples_sent(1) > 0);
}

TEST_CASE("Multi-channel run configures per-channel RF settings", "[integration][multi_channel]") {
    auto device = std::make_shared<StubDevice>();
    RuntimeConfig config;
    config.channel = 0;

    Runtime rt(device, config);

    Plan plan;
    plan.channels.push_back(ChannelBinding{
        "stub0", 0, RfSettings{1e9, 10e6, 20.0, 8e6, "TX/RX"}
    });
    plan.channels.push_back(ChannelBinding{
        "stub0", 1, RfSettings{2.4e9, 20e6, 30.0, 16e6, "TX2"}
    });

    RenderInstruction instr;
    instr.emitter_id = "cw1";
    instr.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"frequency_hz", 1000.0}, {"amplitude", 0.5}}};
    instr.duration_sec = 0.001;
    instr.sample_rate = 10e6;
    plan.render_instructions.push_back(instr);

    REQUIRE(rt.prepare(plan));

    const auto& history = device->call_history();
    bool found_freq0 = false, found_freq1 = false;
    bool found_gain0 = false, found_gain1 = false;
    bool found_bw0 = false, found_bw1 = false;

    for (const auto& call : history) {
        if (call.method == "set_center_freq") {
            if (call.channel == 0 && call.value == 1e9) found_freq0 = true;
            if (call.channel == 1 && call.value == 2.4e9) found_freq1 = true;
        }
        if (call.method == "set_gain") {
            if (call.channel == 0 && call.value == 20.0) found_gain0 = true;
            if (call.channel == 1 && call.value == 30.0) found_gain1 = true;
        }
        if (call.method == "set_bandwidth") {
            if (call.channel == 0 && call.value == 8e6) found_bw0 = true;
            if (call.channel == 1 && call.value == 16e6) found_bw1 = true;
        }
    }

    REQUIRE(found_freq0);
    REQUIRE(found_freq1);
    REQUIRE(found_gain0);
    REQUIRE(found_gain1);
    REQUIRE(found_bw0);
    REQUIRE(found_bw1);
}

TEST_CASE("Multi-channel run starts and stops TX on all channels", "[integration][multi_channel]") {
    auto device = std::make_shared<StubDevice>();
    RuntimeConfig config;
    config.queue_capacity = 32;
    config.block_size = 4096;
    config.channel = 0;

    Runtime rt(device, config);

    Plan plan;
    plan.channels.push_back(ChannelBinding{"stub0", 0, RfSettings{1e9, 1e6, 20.0}});
    plan.channels.push_back(ChannelBinding{"stub0", 1, RfSettings{2.4e9, 1e6, 25.0}});

    RenderInstruction instr;
    instr.emitter_id = "cw1";
    instr.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"frequency_hz", 1000.0}, {"amplitude", 0.5}}};
    instr.duration_sec = 0.001;
    instr.sample_rate = 1e6;
    plan.render_instructions.push_back(instr);

    plan.estimated_duration_sec = 0.001;

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());

    const auto& history = device->call_history();
    int start_tx_0 = 0, start_tx_1 = 0;
    int stop_tx_0 = 0, stop_tx_1 = 0;

    for (const auto& call : history) {
        if (call.method == "start_tx") {
            if (call.channel == 0) start_tx_0++;
            if (call.channel == 1) start_tx_1++;
        }
        if (call.method == "stop_tx") {
            if (call.channel == 0) stop_tx_0++;
            if (call.channel == 1) stop_tx_1++;
        }
    }

    REQUIRE(start_tx_0 >= 1);
    REQUIRE(start_tx_1 >= 1);
    REQUIRE(stop_tx_0 >= 1);
    REQUIRE(stop_tx_1 >= 1);
}

TEST_CASE("Multi-channel aggregate metrics match per-channel sum", "[integration][multi_channel]") {
    auto device = std::make_shared<StubDevice>();
    RuntimeConfig config;
    config.queue_capacity = 32;
    config.block_size = 4096;
    config.channel = 0;

    Runtime rt(device, config);

    Plan plan;
    plan.channels.push_back(ChannelBinding{"stub0", 0, RfSettings{1e9, 1e6, 20.0}});
    plan.channels.push_back(ChannelBinding{"stub0", 1, RfSettings{2.4e9, 1e6, 25.0}});

    RenderInstruction instr;
    instr.emitter_id = "cw1";
    instr.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"frequency_hz", 1000.0}, {"amplitude", 0.5}}};
    instr.duration_sec = 0.005;
    instr.sample_rate = 1e6;
    plan.render_instructions.push_back(instr);

    plan.estimated_duration_sec = 0.005;

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());

    auto metrics = rt.get_metrics();
    REQUIRE(metrics.per_channel.size() == 2);

    size_t per_ch_samples_sum = metrics.per_channel[0].samples_sent + metrics.per_channel[1].samples_sent;
    size_t per_ch_blocks_sum = metrics.per_channel[0].blocks_sent + metrics.per_channel[1].blocks_sent;

    REQUIRE(metrics.total_samples_sent == per_ch_samples_sum);
    REQUIRE(metrics.total_blocks_sent == per_ch_blocks_sum);
}
