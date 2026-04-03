#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/common/error.hpp"
#include "archerfish/scenario/scenario.hpp"

namespace archerfish::scenario {

struct ChannelBinding {
    std::string device_id;
    uint32_t channel_index{0};
    RfSettings rf;
};

enum class TimelineEventType {
    EmitterStart,
    EmitterStop,
    GainChange,
    FreqChange,
    Marker
};

struct TimelineEvent {
    TimelineEventType type;
    double time_sec{0.0};
    std::string target_id;
    nlohmann::json payload;
};

struct RenderInstruction {
    std::string emitter_id;
    WaveformDef waveform;
    double start_sec{0.0};
    double duration_sec{0.0};
    double sample_rate{0.0};
    std::optional<double> resample_ratio;
    std::optional<ImpairmentSettings> impairments;
};

struct Plan {
    Scenario normalized_scenario;
    std::vector<ChannelBinding> channels;
    std::vector<TimelineEvent> timeline;
    std::vector<RenderInstruction> render_instructions;
    common::ErrorList warnings;
    double estimated_duration_sec{0.0};
};

} // namespace archerfish::scenario
