#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>

#include <archerfish/runtime/runtime.hpp>
#include <archerfish/hal/stub_device.hpp>
#include <archerfish/scenario/planner.hpp>

using namespace archerfish::runtime;
using namespace archerfish::hal;

namespace {

class ThrowingFirstChannelStopDevice : public StubDevice {
public:
    void stop_tx(uint32_t channel) override {
        if (channel == 0) {
            throw std::runtime_error("first channel stop failed");
        }
        StubDevice::stop_tx(channel);
    }
};

} // namespace

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

static archerfish::scenario::Plan make_single_nonzero_channel_plan(
    archerfish::scenario::RunMode mode = archerfish::scenario::RunMode::Realtime) {
    archerfish::scenario::Scenario s;
    s.metadata.name = "single_ch1_runtime";
    s.devices.push_back({"stub0", 1, {1e9, 1e6, 10.0}});
    s.run.mode = mode;

    archerfish::scenario::EmitterDef em;
    em.id = "em0";
    em.device = "stub0";
    em.channel = 1;
    em.start_after_sec = 0.0;
    em.duration_sec = 0.001;
    em.waveform = archerfish::scenario::WaveformDef{
        std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}};
    s.emitters.push_back(em);

    auto result = archerfish::scenario::plan(s);
    REQUIRE(result.has_value());
    return *result;
}

static archerfish::scenario::Plan make_two_active_channel_plan() {
    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    p.channels.push_back({"stub0", 1, {1e9, 1e6, 10.0}});

    archerfish::scenario::WaveformDef cw{
        std::nullopt,
        archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.2}},
    };
    p.render_instructions.push_back({
        .emitter_id = "em0",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.001,
        .sample_rate = 1e6,
    });
    p.render_instructions.push_back({
        .emitter_id = "em1",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.001,
        .sample_rate = 1e6,
    });
    return p;
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

TEST_CASE("Multi-channel runtime rejects duplicated channel-plan render membership", "[runtime][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);
    auto p = make_two_active_channel_plan();

    archerfish::scenario::ChannelPlan ch0_plan;
    ch0_plan.channel_id = "stub0:0";
    ch0_plan.channel_index = 0;
    ch0_plan.rf = p.channels[0].rf;
    ch0_plan.render_instructions = {p.render_instructions[0]};

    archerfish::scenario::ChannelPlan ch1_plan;
    ch1_plan.channel_id = "stub0:1";
    ch1_plan.channel_index = 1;
    ch1_plan.rf = p.channels[1].rf;
    ch1_plan.render_instructions = {p.render_instructions[0]};

    p.channel_plans = {ch0_plan, ch1_plan};

    REQUIRE(rt.prepare(p));
    REQUIRE_FALSE(rt.arm());
}

TEST_CASE("Multi-channel runtime rejects missing channel-plan render membership", "[runtime][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);
    auto p = make_two_active_channel_plan();

    archerfish::scenario::ChannelPlan ch0_plan;
    ch0_plan.channel_id = "stub0:0";
    ch0_plan.channel_index = 0;
    ch0_plan.rf = p.channels[0].rf;
    ch0_plan.render_instructions = {p.render_instructions[0]};

    archerfish::scenario::ChannelPlan ch1_plan;
    ch1_plan.channel_id = "stub0:1";
    ch1_plan.channel_index = 1;
    ch1_plan.rf = p.channels[1].rf;

    p.channel_plans = {ch0_plan, ch1_plan};

    REQUIRE(rt.prepare(p));
    REQUIRE_FALSE(rt.arm());
}

TEST_CASE("Multi-channel runtime rejects empty channel-plan metadata event strings", "[runtime][multi_channel][events]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);
    auto p = make_two_active_channel_plan();

    archerfish::scenario::ChannelPlan ch0_plan;
    ch0_plan.channel_id = "stub0:0";
    ch0_plan.channel_index = 0;
    ch0_plan.rf = p.channels[0].rf;
    ch0_plan.render_instructions = {p.render_instructions[0]};
    ch0_plan.events.push_back({archerfish::scenario::TimelineEventType::WaveformSwitch,
                               0.0,
                               "stub0",
                               nlohmann::json{{"emitter_id", "em0"}, {"new_waveform", ""}}});

    archerfish::scenario::ChannelPlan ch1_plan;
    ch1_plan.channel_id = "stub0:1";
    ch1_plan.channel_index = 1;
    ch1_plan.rf = p.channels[1].rf;
    ch1_plan.render_instructions = {p.render_instructions[1]};

    p.channel_plans = {ch0_plan, ch1_plan};

    REQUIRE(rt.prepare(p));
    REQUIRE_FALSE(rt.arm());
}

TEST_CASE("Multi-channel runtime rejects malformed channel-plan RF event channels", "[runtime][multi_channel][events]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);
    auto p = make_two_active_channel_plan();

    archerfish::scenario::ChannelPlan ch0_plan;
    ch0_plan.channel_id = "stub0:0";
    ch0_plan.channel_index = 0;
    ch0_plan.rf = p.channels[0].rf;
    ch0_plan.render_instructions = {p.render_instructions[0]};
    ch0_plan.events.push_back({archerfish::scenario::TimelineEventType::GainChange,
                               0.0,
                               "stub0",
                               nlohmann::json{{"gain_db", 12.0}, {"channel", "zero"}}});

    archerfish::scenario::ChannelPlan ch1_plan;
    ch1_plan.channel_id = "stub0:1";
    ch1_plan.channel_index = 1;
    ch1_plan.rf = p.channels[1].rf;
    ch1_plan.render_instructions = {p.render_instructions[1]};

    p.channel_plans = {ch0_plan, ch1_plan};

    REQUIRE(rt.prepare(p));
    REQUIRE_FALSE(rt.arm());
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

TEST_CASE("Single active nonzero channel transmits on that channel", "[runtime][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_single_nonzero_channel_plan();

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());

    REQUIRE(dev->total_samples_sent(1) > 0);
    REQUIRE(dev->total_samples_sent(0) == 0);
    const auto& calls = dev->call_history();
    REQUIRE(std::any_of(calls.begin(), calls.end(), [](const DeviceCallRecord& call) {
        return call.method == "start_tx" && call.channel == 1;
    }));
    REQUIRE(std::none_of(calls.begin(), calls.end(), [](const DeviceCallRecord& call) {
        return call.method == "send_samples" && call.channel == 0;
    }));
}

TEST_CASE("Replay mode single active nonzero channel transmits on that channel", "[runtime][multi_channel][replay]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_single_nonzero_channel_plan(archerfish::scenario::RunMode::Replay);

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());

    REQUIRE(dev->total_samples_sent(1) > 0);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Replay mode completes zero-sample jobs without starting TX", "[runtime][multi_channel][replay]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_single_nonzero_channel_plan(archerfish::scenario::RunMode::Replay);
    REQUIRE_FALSE(p.render_instructions.empty());
    p.render_instructions[0].duration_sec = 0.0;

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);
    REQUIRE(dev->total_samples_sent(1) == 0);
    REQUIRE_FALSE(dev->is_tx_active(1));

    const auto& calls = dev->call_history();
    REQUIRE(std::none_of(calls.begin(), calls.end(), [](const DeviceCallRecord& call) {
        return call.method == "start_tx";
    }));
}

TEST_CASE("Multi-channel runtime does not let a zero-sample channel cancel active channels",
          "[runtime][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_two_active_channel_plan();
    p.render_instructions[0].duration_sec = 0.0;

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Completed);
    REQUIRE(dev->total_samples_sent(0) == 0);
    REQUIRE(dev->total_samples_sent(1) == 1000);

    const auto metrics = rt.get_metrics();
    REQUIRE(metrics.per_channel.size() == 2);
    REQUIRE(metrics.per_channel[0].samples_sent == 0);
    REQUIRE(metrics.per_channel[1].samples_sent == 1000);
}

TEST_CASE("Replay mode fails when a render job cannot pre-render", "[runtime][multi_channel][replay]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_single_nonzero_channel_plan(archerfish::scenario::RunMode::Replay);
    REQUIRE_FALSE(p.render_instructions.empty());
    p.render_instructions[0].waveform.params["amplitude"] = -0.1;

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(1) == 0);
}

TEST_CASE("Replay mode rejects multiple active channels", "[runtime][multi_channel][replay]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_two_active_channel_plan();
    p.run_mode = archerfish::scenario::RunMode::Replay;

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
    REQUIRE(dev->total_samples_sent(1) == 0);
}

TEST_CASE("Multi-channel runtime stops later channels when one stop fails", "[runtime][multi_channel]") {
    auto dev = std::make_shared<ThrowingFirstChannelStopDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_two_active_channel_plan();

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->is_tx_active(0));
    REQUIRE_FALSE(dev->is_tx_active(1));

    const auto& calls = dev->call_history();
    REQUIRE(std::any_of(calls.begin(), calls.end(), [](const DeviceCallRecord& call) {
        return call.method == "stop_tx" && call.channel == 1;
    }));
}

TEST_CASE("Runtime skips stale mix groups with no matching render jobs", "[runtime][mixing]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_single_nonzero_channel_plan();
    p.mix_groups.push_back(archerfish::scenario::MixGroup{
        .device_id = "stub0",
        .channel = 1,
        .start_sec = 0.002,
        .duration_sec = 0.001,
        .emitter_ids = {"missing_emitter"},
        .estimated_peak_sum = 0.0,
    });

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());
    REQUIRE(dev->total_samples_sent(1) > 0);
}

TEST_CASE("Runtime mix groups round total sample count like DSP sources", "[runtime][mixing]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});

    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0", 0, {1e9, 1000.0, 10.0}});

    archerfish::scenario::WaveformDef cw{
        std::nullopt,
        archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.2}, {"frequency_hz", 0.0}},
    };
    p.render_instructions.push_back({
        .emitter_id = "em0",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.0015,
        .sample_rate = 1000.0,
    });
    p.render_instructions.push_back({
        .emitter_id = "em1",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.0015,
        .sample_rate = 1000.0,
    });
    p.mix_groups.push_back({
        .device_id = "stub0",
        .channel = 0,
        .start_sec = 0.0,
        .duration_sec = 0.0015,
        .emitter_ids = {"em0", "em1"},
        .estimated_peak_sum = 0.4,
    });

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());
    REQUIRE(dev->total_samples_sent(0) == 2);
}

TEST_CASE("Runtime executes mix groups and solo jobs by scheduled start time", "[runtime][mixing]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 8});

    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0", 0, {1e9, 1000.0, 10.0}});

    archerfish::scenario::WaveformDef cw{
        std::nullopt,
        archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.2}, {"frequency_hz", 0.0}},
    };
    p.render_instructions.push_back({
        .emitter_id = "solo",
        .waveform = cw,
        .start_sec = 0.010,
        .duration_sec = 0.001,
        .sample_rate = 1000.0,
    });
    p.render_instructions.push_back({
        .emitter_id = "mix0",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.002,
        .sample_rate = 1000.0,
    });
    p.render_instructions.push_back({
        .emitter_id = "mix1",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.002,
        .sample_rate = 1000.0,
    });
    p.mix_groups.push_back({
        .device_id = "stub0",
        .channel = 0,
        .start_sec = 0.0,
        .duration_sec = 0.002,
        .emitter_ids = {"mix0", "mix1"},
        .estimated_peak_sum = 0.4,
    });

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());

    const auto calls = dev->call_history();
    auto first_send = std::find_if(calls.begin(), calls.end(), [](const DeviceCallRecord& call) {
        return call.method == "send_samples" && call.sample_count > 0;
    });
    REQUIRE(first_send != calls.end());
    REQUIRE(first_send->sample_count == 2);
    REQUIRE(dev->total_samples_sent(0) == 3);
}

TEST_CASE("Runtime skips stale multi-channel mix groups with no matching render jobs", "[runtime][mixing][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_two_active_channel_plan();
    p.mix_groups.push_back(archerfish::scenario::MixGroup{
        .device_id = "stub0",
        .channel = 1,
        .start_sec = 0.002,
        .duration_sec = 0.001,
        .emitter_ids = {"missing_emitter"},
        .estimated_peak_sum = 0.0,
    });

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());
    REQUIRE(dev->total_samples_sent(0) > 0);
    REQUIRE(dev->total_samples_sent(1) > 0);
}

TEST_CASE("Multi-channel runtime does not also transmit mix members as solo jobs", "[runtime][mixing][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 2});

    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0", 0, {1e9, 1000.0, 10.0}});
    p.channels.push_back({"stub0", 1, {1e9, 1000.0, 10.0}});

    archerfish::scenario::WaveformDef cw{
        std::nullopt,
        archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.2}, {"frequency_hz", 0.0}},
    };
    archerfish::scenario::RenderInstruction mix0{
        .emitter_id = "mix0",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.002,
        .sample_rate = 1000.0,
    };
    archerfish::scenario::RenderInstruction mix1{
        .emitter_id = "mix1",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.002,
        .sample_rate = 1000.0,
    };
    archerfish::scenario::RenderInstruction ch1{
        .emitter_id = "ch1",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.001,
        .sample_rate = 1000.0,
    };
    p.render_instructions = {mix0, mix1, ch1};

    archerfish::scenario::ChannelPlan ch0_plan;
    ch0_plan.channel_id = "stub0:0";
    ch0_plan.channel_index = 0;
    ch0_plan.rf = p.channels[0].rf;
    ch0_plan.render_instructions = {mix0, mix1};

    archerfish::scenario::ChannelPlan ch1_plan;
    ch1_plan.channel_id = "stub0:1";
    ch1_plan.channel_index = 1;
    ch1_plan.rf = p.channels[1].rf;
    ch1_plan.render_instructions = {ch1};

    p.channel_plans = {ch0_plan, ch1_plan};
    p.mix_groups.push_back({
        .device_id = "stub0",
        .channel = 0,
        .start_sec = 0.0,
        .duration_sec = 0.002,
        .emitter_ids = {"mix0", "mix1"},
        .estimated_peak_sum = 0.4,
    });

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());

    REQUIRE(dev->total_samples_sent(0) == 2);
    REQUIRE(dev->total_samples_sent(1) == 1);
}

TEST_CASE("Multi-channel runtime fails when any channel render job fails", "[runtime][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});
    auto p = make_two_active_channel_plan();
    REQUIRE_FALSE(p.render_instructions.empty());
    for (auto& instr : p.render_instructions) {
        instr.waveform.params["amplitude"] = -0.1;
    }

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
    REQUIRE(dev->total_samples_sent(1) == 0);
}

TEST_CASE("Multi-channel runtime fails when a mix group sample count is invalid", "[runtime][mixing][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 128});

    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    p.channels.push_back({"stub0", 1, {1e9, 1e6, 10.0}});

    const archerfish::scenario::WaveformDef cw{
        std::nullopt,
        archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.2}},
    };
    archerfish::scenario::RenderInstruction mix0{
        .emitter_id = "mix0",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 2.0,
        .sample_rate = std::numeric_limits<double>::max(),
    };
    archerfish::scenario::RenderInstruction mix1{
        .emitter_id = "mix1",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 2.0,
        .sample_rate = std::numeric_limits<double>::max(),
    };
    archerfish::scenario::RenderInstruction ch1{
        .emitter_id = "ch1",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.001,
        .sample_rate = 1000.0,
    };
    p.render_instructions = {mix0, mix1, ch1};

    archerfish::scenario::ChannelPlan ch0_plan;
    ch0_plan.channel_id = "stub0:0";
    ch0_plan.channel_index = 0;
    ch0_plan.rf = p.channels[0].rf;
    ch0_plan.render_instructions = {mix0, mix1};

    archerfish::scenario::ChannelPlan ch1_plan;
    ch1_plan.channel_id = "stub0:1";
    ch1_plan.channel_index = 1;
    ch1_plan.rf = p.channels[1].rf;
    ch1_plan.render_instructions = {ch1};

    p.channel_plans = {ch0_plan, ch1_plan};
    p.mix_groups.push_back({
        .device_id = "stub0",
        .channel = 0,
        .start_sec = 0.0,
        .duration_sec = 2.0,
        .emitter_ids = {"mix0", "mix1"},
        .estimated_peak_sum = 0.4,
    });

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Multi-channel runtime matches channel plans by channel index", "[runtime][multi_channel]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 2});

    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0", 0, {1e9, 1000.0, 10.0}});
    p.channels.push_back({"stub0", 1, {1e9, 1000.0, 10.0}});

    archerfish::scenario::WaveformDef cw{
        std::nullopt,
        archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.2}, {"frequency_hz", 0.0}},
    };
    archerfish::scenario::RenderInstruction ch0_instr{
        .emitter_id = "ch0_em",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.001,
        .sample_rate = 1000.0,
    };
    archerfish::scenario::RenderInstruction ch1_instr{
        .emitter_id = "ch1_em",
        .waveform = cw,
        .start_sec = 0.0,
        .duration_sec = 0.003,
        .sample_rate = 1000.0,
    };
    p.render_instructions = {ch0_instr, ch1_instr};

    archerfish::scenario::ChannelPlan ch1_plan;
    ch1_plan.channel_id = "stub0:1";
    ch1_plan.channel_index = 1;
    ch1_plan.rf = p.channels[1].rf;
    ch1_plan.render_instructions = {ch1_instr};

    archerfish::scenario::ChannelPlan ch0_plan;
    ch0_plan.channel_id = "stub0:0";
    ch0_plan.channel_index = 0;
    ch0_plan.rf = p.channels[0].rf;
    ch0_plan.render_instructions = {ch0_instr};

    p.channel_plans = {ch1_plan, ch0_plan};

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());

    REQUIRE(dev->total_samples_sent(0) == 1);
    REQUIRE(dev->total_samples_sent(1) == 3);
}

TEST_CASE("Multi-channel runtime dispatches channel plan RF events", "[runtime][multi_channel][events]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 4, .block_size = 2});

    auto p = make_two_active_channel_plan();
    REQUIRE(p.render_instructions.size() == 2);

    archerfish::scenario::TimelineEvent gain_event{
        archerfish::scenario::TimelineEventType::GainChange,
        0.0,
        "stub0",
        nlohmann::json{{"gain_db", 42.0}},
    };

    archerfish::scenario::ChannelPlan ch0_plan;
    ch0_plan.channel_id = "stub0:0";
    ch0_plan.channel_index = 0;
    ch0_plan.rf = p.channels[0].rf;
    ch0_plan.render_instructions = {p.render_instructions[0]};
    ch0_plan.events = {gain_event};

    archerfish::scenario::ChannelPlan ch1_plan;
    ch1_plan.channel_id = "stub0:1";
    ch1_plan.channel_index = 1;
    ch1_plan.rf = p.channels[1].rf;
    ch1_plan.render_instructions = {p.render_instructions[1]};
    ch1_plan.events = {gain_event};

    p.channel_plans = {ch0_plan, ch1_plan};

    REQUIRE(rt.prepare(p));
    REQUIRE(rt.arm());
    REQUIRE(rt.run());

    const auto& calls = dev->call_history();
    REQUIRE(std::any_of(calls.begin(), calls.end(), [](const DeviceCallRecord& call) {
        return call.method == "set_gain" && call.channel == 0 && call.value == 42.0;
    }));
    REQUIRE(std::any_of(calls.begin(), calls.end(), [](const DeviceCallRecord& call) {
        return call.method == "set_gain" && call.channel == 1 && call.value == 42.0;
    }));
}
