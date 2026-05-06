#include <catch2/catch_test_macros.hpp>

#include "archerfish/runtime/runtime.hpp"
#include "archerfish/runtime/state.hpp"
#include "archerfish/hal/stub_device.hpp"
#include "archerfish/scenario/planner.hpp"

#include <algorithm>
#include <complex>
#include <limits>
#include <stdexcept>

using namespace archerfish::runtime;
using namespace archerfish::hal;

namespace {

class ThrowingStartDevice : public StubDevice {
public:
    void start_tx(uint32_t channel) override {
        (void)channel;
        throw std::runtime_error("start failed");
    }
};

class ThrowingSecondChannelStartDevice : public StubDevice {
public:
    void start_tx(uint32_t channel) override {
        if (channel == 1) {
            throw std::runtime_error("second channel start failed");
        }
        StubDevice::start_tx(channel);
    }
};

class ThrowingStopDevice : public StubDevice {
public:
    void stop_tx(uint32_t channel) override {
        if (channel == 0) {
            throw std::runtime_error("stop failed");
        }
        StubDevice::stop_tx(channel);
    }
};

class ThrowingAfterFirstSendDevice : public StubDevice {
public:
    size_t send_samples(uint32_t channel,
                        const std::complex<float>* data,
                        size_t count,
                        const TxMetadata& meta) override {
        if (send_calls_++ > 0) {
            throw std::runtime_error("send failed");
        }
        return StubDevice::send_samples(channel, data, count, meta);
    }

private:
    size_t send_calls_{0};
};

class PartialSendDevice : public StubDevice {
public:
    size_t send_samples(uint32_t channel,
                        const std::complex<float>* data,
                        size_t count,
                        const TxMetadata& meta) override {
        const size_t accepted = count / 2;
        return StubDevice::send_samples(channel, data, accepted, meta);
    }
};

class ThrowingGainDevice : public StubDevice {
public:
    void set_gain(uint32_t channel, double gain_db) override {
        if (gain_calls_++ > 0) {
            throw std::runtime_error("scheduled gain failed");
        }
        StubDevice::set_gain(channel, gain_db);
    }

private:
    size_t gain_calls_{0};
};

archerfish::scenario::Plan make_single_channel_runtime_plan() {
    archerfish::scenario::Scenario s;
    s.metadata.name = "single_channel_runtime";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 0.005,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());
    return *plan_result;
}

} // namespace

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

TEST_CASE("Runtime prepare fails cleanly without a HAL device", "[runtime][lifecycle]") {
    Runtime rt(nullptr);
    archerfish::scenario::Plan p;

    REQUIRE_FALSE(rt.prepare(p));
    REQUIRE(rt.state() == RuntimeState::Failed);
}

TEST_CASE("Runtime abort is safe without a HAL device", "[runtime][lifecycle]") {
    Runtime rt(nullptr);
    rt.abort();
    REQUIRE(rt.state() == RuntimeState::Aborted);
}

TEST_CASE("Runtime prepare rejects zero queue capacity", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 0, .block_size = 1024});
    archerfish::scenario::Plan p;

    REQUIRE_FALSE(rt.prepare(p));
    REQUIRE(rt.state() == RuntimeState::Failed);
}

TEST_CASE("Runtime prepare rejects invalid direct plan RF settings before HAL calls", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);
    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0",
                          0,
                          {std::numeric_limits<double>::quiet_NaN(),
                           1e6,
                           10.0}});

    REQUIRE_FALSE(rt.prepare(p));
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->call_history().empty());
}

TEST_CASE("Runtime prepare rejects duplicate runtime channel indices before HAL calls", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);

    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    p.channels.push_back({"stub0", 0, {2e9, 1e6, 12.0}});

    REQUIRE_FALSE(rt.prepare(p));
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->call_history().empty());
}

TEST_CASE("Runtime prepare rejects multi-device plans before HAL calls", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);

    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    p.channels.push_back({"stub1", 1, {2e9, 1e6, 12.0}});

    REQUIRE_FALSE(rt.prepare(p));
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->call_history().empty());
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

TEST_CASE("Runtime arm rejects an empty render plan before arming", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev);
    archerfish::scenario::Plan p;
    p.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});

    REQUIRE(rt.prepare(p));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
}

TEST_CASE("Runtime arm rejects zero block size", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, RuntimeConfig{.queue_capacity = 16, .block_size = 0});

    archerfish::scenario::Scenario s;
    s.metadata.name = "zero_block";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 0.01,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.5}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());
    REQUIRE(rt.prepare(*plan_result));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
}

TEST_CASE("Runtime arm rejects mix groups with unknown emitter ids", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.mix_groups.push_back({"stub0", 0, 0.0, 0.001, {"missing"}, 0.3});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects duplicate render instruction ids", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.2}}},
        0.002,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects overlapping render instructions without a mix group", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.002,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.render_instructions.push_back({
        "em1",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.2}}},
        0.001,
        0.002,
        1e6,
        std::nullopt,
        std::nullopt,
    });

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm accepts overlapping render instructions covered by one mix group", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.002,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.render_instructions.push_back({
        "em1",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.2}}},
        0.001,
        0.002,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.mix_groups.push_back({"stub0", 0, 0.0, 0.003, {"em0", "em1"}, 0.5});

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Armed);
}

TEST_CASE("Runtime run fails when a mix group sample count is invalid", "[runtime][lifecycle][mixing]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 4, .block_size = 128});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    const archerfish::scenario::WaveformDef cw{
        std::nullopt,
        archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.2}},
    };
    plan.render_instructions.push_back({
        "em0",
        cw,
        0.0,
        2.0,
        std::numeric_limits<double>::max(),
        std::nullopt,
        std::nullopt,
    });
    plan.render_instructions.push_back({
        "em1",
        cw,
        0.0,
        2.0,
        std::numeric_limits<double>::max(),
        std::nullopt,
        std::nullopt,
    });
    plan.mix_groups.push_back({"stub0", 0, 0.0, 2.0, {"em0", "em1"}, 0.4});

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects mix groups that overlap solo render instructions", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "mix0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.render_instructions.push_back({
        "solo",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.2}}},
        0.001,
        0.002,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.mix_groups.push_back({"stub0", 0, 0.0, 0.003, {"mix0"}, 0.3});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects duplicate mix group members", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.mix_groups.push_back({"stub0", 0, 0.0, 0.001, {"em0", "em0"}, 0.6});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects overlapping mix groups on one channel", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.render_instructions.push_back({
        "em1",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.2}}},
        0.002,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.mix_groups.push_back({"stub0", 0, 0.0, 0.003, {"em0"}, 0.3});
    plan.mix_groups.push_back({"stub0", 0, 0.001, 0.003, {"em1"}, 0.2});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects mix groups with unknown channels", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.mix_groups.push_back({"stub0", 1, 0.0, 0.001, {"em0"}, 0.3});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
    REQUIRE(dev->total_samples_sent(1) == 0);
}

TEST_CASE("Runtime arm rejects dangling channel plans", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.channel_plans.push_back({"ch1", 1, {1e9, 1e6, 10.0}, {}, {}, {}});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects duplicate channel plans", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.channel_plans.push_back({"ch0a", 0, {1e9, 1e6, 10.0}, {}, {}, {}});
    plan.channel_plans.push_back({"ch0b", 0, {1e9, 1e6, 10.0}, {}, {}, {}});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects duplicate channel plan render instructions", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    archerfish::scenario::RenderInstruction instr{
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    };
    plan.render_instructions.push_back(instr);
    plan.channel_plans.push_back({"ch0", 0, {1e9, 1e6, 10.0}, {instr, instr}, {}, {}});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects dangling channel plan render instructions", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    archerfish::scenario::RenderInstruction top_level{
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    };
    auto dangling = top_level;
    dangling.emitter_id = "missing";
    plan.render_instructions.push_back(top_level);
    plan.channel_plans.push_back({"ch0", 0, {1e9, 1e6, 10.0}, {dangling}, {}, {}});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects channel plan event with mismatched channel payload", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    archerfish::scenario::RenderInstruction instr{
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    };
    plan.render_instructions.push_back(instr);
    archerfish::scenario::TimelineEvent event{
        archerfish::scenario::TimelineEventType::GainChange,
        0.0,
        "stub0",
        nlohmann::json{{"gain_db", 12.0}, {"channel", 1}},
    };
    plan.channel_plans.push_back({"ch0", 0, {1e9, 1e6, 10.0}, {instr}, {event}, {}});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
}

TEST_CASE("Runtime arm rejects channel plan event with dangling payload emitter", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    archerfish::scenario::RenderInstruction instr{
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    };
    plan.render_instructions.push_back(instr);
    archerfish::scenario::TimelineEvent event{
        archerfish::scenario::TimelineEventType::WaveformSwitch,
        0.0,
        "stub0",
        nlohmann::json{{"emitter_id", "missing"}, {"new_waveform", "next"}},
    };
    plan.channel_plans.push_back({"ch0", 0, {1e9, 1e6, 10.0}, {instr}, {event}, {}});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
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

TEST_CASE("Runtime run catches HAL start failures", "[runtime][lifecycle]") {
    auto dev = std::make_shared<ThrowingStartDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Scenario s;
    s.metadata.name = "run_start_failure";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 0.005,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());

    REQUIRE(rt.prepare(*plan_result));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
}

TEST_CASE("Runtime run fails cleanly when single-channel stop fails", "[runtime][lifecycle]") {
    auto dev = std::make_shared<ThrowingStopDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
    auto plan = make_single_channel_runtime_plan();

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);

    const auto metrics = rt.get_metrics();
    REQUIRE(metrics.actual_stop_sec >= metrics.actual_start_sec);
    REQUIRE(metrics.actual_duration_sec >= 0.0);
}

TEST_CASE("Runtime replay fails cleanly when stop fails", "[runtime][lifecycle][replay]") {
    auto dev = std::make_shared<ThrowingStopDevice>();
    Runtime rt(dev, {.queue_capacity = 16,
                     .block_size = 1024,
                     .run_mode = archerfish::scenario::RunMode::Replay});
    auto plan = make_single_channel_runtime_plan();

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);

    const auto metrics = rt.get_metrics();
    REQUIRE(metrics.actual_stop_sec >= metrics.actual_start_sec);
    REQUIRE(metrics.actual_duration_sec >= 0.0);
}

TEST_CASE("Runtime run fails when single-channel TX send fails after partial progress", "[runtime][lifecycle]") {
    auto dev = std::make_shared<ThrowingAfterFirstSendDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 256});
    auto plan = make_single_channel_runtime_plan();

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) > 0);

    const auto metrics = rt.get_metrics();
    REQUIRE(metrics.total_samples_sent > 0);
}

TEST_CASE("Runtime run fails when single-channel TX sends only part of a block", "[runtime][lifecycle]") {
    auto dev = std::make_shared<PartialSendDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 256});
    auto plan = make_single_channel_runtime_plan();

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) > 0);

    const auto metrics = rt.get_metrics();
    REQUIRE(metrics.total_samples_sent > 0);
    REQUIRE(metrics.underruns > 0);
}

TEST_CASE("Runtime replay fails when TX send fails after partial progress", "[runtime][lifecycle][replay]") {
    auto dev = std::make_shared<ThrowingAfterFirstSendDevice>();
    Runtime rt(dev, {.queue_capacity = 16,
                     .block_size = 256,
                     .run_mode = archerfish::scenario::RunMode::Replay});
    auto plan = make_single_channel_runtime_plan();

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) > 0);

    const auto metrics = rt.get_metrics();
    REQUIRE(metrics.total_samples_sent > 0);
}

TEST_CASE("Runtime run fails when a scheduled HAL event fails", "[runtime][lifecycle][events]") {
    auto dev = std::make_shared<ThrowingGainDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
    auto plan = make_single_channel_runtime_plan();
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::GainChange,
                             0.0,
                             "stub0",
                             nlohmann::json{{"gain_db", 12.0}}});

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) > 0);
}

TEST_CASE("Runtime replay fails when a scheduled HAL event fails", "[runtime][lifecycle][events][replay]") {
    auto dev = std::make_shared<ThrowingGainDevice>();
    Runtime rt(dev, {.queue_capacity = 16,
                     .block_size = 1024,
                     .run_mode = archerfish::scenario::RunMode::Replay});
    auto plan = make_single_channel_runtime_plan();
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::GainChange,
                             0.0,
                             "stub0",
                             nlohmann::json{{"gain_db", 12.0}}});

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) > 0);
}

TEST_CASE("Runtime abort swallows HAL stop failures", "[runtime][lifecycle]") {
    auto dev = std::make_shared<ThrowingStopDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
    auto plan = make_single_channel_runtime_plan();

    REQUIRE(rt.prepare(plan));
    REQUIRE_NOTHROW(rt.abort());
    REQUIRE(rt.state() == RuntimeState::Aborted);
}

TEST_CASE("Runtime stops started multi-channel TX when a later channel start fails", "[runtime][lifecycle]") {
    auto dev = std::make_shared<ThrowingSecondChannelStartDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.channels.push_back({"stub0", 1, {1.1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.render_instructions.push_back({
        "em1",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.2}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE_FALSE(dev->is_tx_active(0));
    REQUIRE_FALSE(dev->is_tx_active(1));

    const auto calls = dev->call_history();
    REQUIRE(std::any_of(calls.begin(), calls.end(), [](const auto& call) {
        return call.method == "stop_tx" && call.channel == 0;
    }));
}

TEST_CASE("Runtime run fails when realtime rendering fails", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Scenario s;
    s.metadata.name = "render_failure";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 0.005,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());
    auto plan = *plan_result;
    plan.render_instructions[0].waveform.params["amplitude"] = -0.1;

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects waveform params with reserved type key", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::QPSK,
                                           nlohmann::json{{"type", "bpsk"},
                                                          {"symbol_rate", 1000.0},
                                                          {"samples_per_symbol", 4}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
}

TEST_CASE("Runtime arm accepts null waveform params", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nullptr},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Armed);
}

TEST_CASE("Runtime rejects mix group members that start before the group", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 4});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1000.0, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1000.0,
        std::nullopt,
        std::nullopt,
    });
    plan.mix_groups.push_back({"stub0", 0, 0.001, 0.001, {"em0"}, 0.3});

    REQUIRE(rt.prepare(plan));
    REQUIRE(rt.arm());
    REQUIRE_FALSE(rt.run());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects timeline events with unknown devices", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::FreqChange,
                             0.0,
                             "missing_device",
                             nlohmann::json{{"freq_hz", 1.1e9}}});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects timeline events with unknown channels", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::GainChange,
                             0.0,
                             "stub0",
                             nlohmann::json{{"gain_db", 20.0}, {"channel", 1}}});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
    REQUIRE(dev->total_samples_sent(1) == 0);
}

TEST_CASE("Runtime arm rejects timeline events with unknown emitters", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Plan plan;
    plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    plan.render_instructions.push_back({
        "em0",
        archerfish::scenario::WaveformDef{std::nullopt,
                                           archerfish::dsp::WaveformType::CW,
                                           nlohmann::json{{"amplitude", 0.3}}},
        0.0,
        0.001,
        1e6,
        std::nullopt,
        std::nullopt,
    });
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::EmitterStart,
                             0.0,
                             "missing_emitter",
                             nlohmann::json::object()});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects malformed RF event channel payloads", "[runtime][lifecycle]") {
    archerfish::scenario::Scenario s;
    s.metadata.name = "invalid_event_channel";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 0.005,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());

    SECTION("negative channel") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = *plan_result;
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::FreqChange,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"freq_hz", 1.1e9}, {"channel", -1}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
    }

    SECTION("string channel") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = *plan_result;
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::GainChange,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"gain_db", 12.0}, {"channel", "zero"}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
    }

    SECTION("out-of-range channel") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = *plan_result;
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::GainChange,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"gain_db", 12.0},
                                                {"channel", static_cast<uint64_t>(
                                                                std::numeric_limits<uint32_t>::max()) + 1u}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
    }
}

TEST_CASE("Runtime arm rejects malformed event payload fields", "[runtime][lifecycle]") {
    auto dev = std::make_shared<StubDevice>();
    Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});

    archerfish::scenario::Scenario s;
    s.metadata.name = "malformed_event_payloads";
    s.devices.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
    s.emitters.push_back({"em0", "stub0", 0, 0.0, 0.005,
        archerfish::scenario::WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW,
        nlohmann::json{{"amplitude", 0.3}}}, std::nullopt, std::nullopt,
        archerfish::scenario::MixingMode::None, std::nullopt, std::nullopt});

    auto plan_result = archerfish::scenario::plan(s);
    REQUIRE(plan_result.has_value());
    auto plan = *plan_result;
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::FreqChange,
                             0.0,
                             "stub0",
                             nlohmann::json{{"freq_hz", "high"}}});
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::GainChange,
                             0.0,
                             "stub0",
                             nlohmann::json{{"gain_db", "loud"}}});
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::Marker,
                             0.0,
                             "stub0",
                             nlohmann::json{{"name", 42}}});
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::WaveformSwitch,
                             0.0,
                             "stub0",
                             nlohmann::json{{"emitter_id", 42}, {"new_waveform", false}}});
    plan.timeline.push_back({archerfish::scenario::TimelineEventType::ImpairmentChange,
                             0.0,
                             "stub0",
                             nlohmann::json{{"emitter_id", 42}, {"impairment", false}, {"enabled", "yes"}}});

    REQUIRE(rt.prepare(plan));
    REQUIRE_FALSE(rt.arm());
    REQUIRE(rt.state() == RuntimeState::Failed);
    REQUIRE(dev->total_samples_sent(0) == 0);
}

TEST_CASE("Runtime arm rejects malformed metadata event payloads", "[runtime][lifecycle]") {
    auto make_plan = [] {
        archerfish::scenario::Plan plan;
        plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
        plan.render_instructions.push_back({
            "em0",
            archerfish::scenario::WaveformDef{std::nullopt,
                                               archerfish::dsp::WaveformType::CW,
                                               nlohmann::json{{"amplitude", 0.3}}},
            0.0,
            0.001,
            1e6,
            std::nullopt,
            std::nullopt,
        });
        return plan;
    };

    SECTION("marker name type") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = make_plan();
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::Marker,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"name", 42}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
        REQUIRE(dev->total_samples_sent(0) == 0);
    }

    SECTION("marker name empty") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = make_plan();
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::Marker,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"name", ""}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
        REQUIRE(dev->total_samples_sent(0) == 0);
    }

    SECTION("waveform switch required strings") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = make_plan();
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::WaveformSwitch,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"emitter_id", 42}, {"new_waveform", false}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
        REQUIRE(dev->total_samples_sent(0) == 0);
    }

    SECTION("waveform switch empty waveform") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = make_plan();
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::WaveformSwitch,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"emitter_id", "em0"},
                                                {"new_waveform", ""}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
        REQUIRE(dev->total_samples_sent(0) == 0);
    }

    SECTION("impairment change required fields") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = make_plan();
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::ImpairmentChange,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"emitter_id", "em0"},
                                                {"impairment", false},
                                                {"enabled", "yes"}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
        REQUIRE(dev->total_samples_sent(0) == 0);
    }

    SECTION("impairment change empty name") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = make_plan();
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::ImpairmentChange,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"emitter_id", "em0"},
                                                {"impairment", ""}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
        REQUIRE(dev->total_samples_sent(0) == 0);
    }
}

TEST_CASE("Runtime arm rejects metadata events with unknown payload emitters", "[runtime][lifecycle]") {
    auto make_plan = [] {
        archerfish::scenario::Plan plan;
        plan.channels.push_back({"stub0", 0, {1e9, 1e6, 10.0}});
        plan.render_instructions.push_back({
            "em0",
            archerfish::scenario::WaveformDef{std::nullopt,
                                               archerfish::dsp::WaveformType::CW,
                                               nlohmann::json{{"amplitude", 0.3}}},
            0.0,
            0.001,
            1e6,
            std::nullopt,
            std::nullopt,
        });
        return plan;
    };

    SECTION("waveform switch emitter") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = make_plan();
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::WaveformSwitch,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"emitter_id", "missing"},
                                                {"new_waveform", "wf2"}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
        REQUIRE(dev->total_samples_sent(0) == 0);
    }

    SECTION("impairment change emitter") {
        auto dev = std::make_shared<StubDevice>();
        Runtime rt(dev, {.queue_capacity = 16, .block_size = 1024});
        auto plan = make_plan();
        plan.timeline.push_back({archerfish::scenario::TimelineEventType::ImpairmentChange,
                                 0.0,
                                 "stub0",
                                 nlohmann::json{{"emitter_id", "missing"},
                                                {"impairment", "cfo_hz"}}});

        REQUIRE(rt.prepare(plan));
        REQUIRE_FALSE(rt.arm());
        REQUIRE(rt.state() == RuntimeState::Failed);
        REQUIRE(dev->total_samples_sent(0) == 0);
    }
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
