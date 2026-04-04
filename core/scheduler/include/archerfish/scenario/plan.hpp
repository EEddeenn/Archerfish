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
    Marker,
    WaveformSwitch,
    ImpairmentChange
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

struct MixGroup {
    std::string device_id;
    uint32_t channel{0};
    double start_sec{0.0};
    double duration_sec{0.0};
    std::vector<std::string> emitter_ids;
    double estimated_peak_sum{0.0};
};

struct ResourceEstimate {
    double estimated_cpu_load{0.0};
    size_t peak_memory_bytes{0};
    double min_inter_emitter_gap_sec{0.0};
    bool timing_feasible{true};
    std::vector<std::string> warnings;
};

struct ChannelPlan {
    std::string channel_id;
    uint32_t channel_index{0};
    RfSettings rf;
    std::vector<RenderInstruction> render_instructions;
    std::vector<TimelineEvent> events;
    ResourceEstimate resource_estimate;
};

struct Plan {
    Scenario normalized_scenario;
    std::vector<ChannelBinding> channels;
    std::vector<TimelineEvent> timeline;
    std::vector<RenderInstruction> render_instructions;
    std::vector<MixGroup> mix_groups;
    ResourceEstimate resource_estimate;
    common::ErrorList warnings;
    double estimated_duration_sec{0.0};
    std::vector<ChannelPlan> channel_plans;
    RunMode run_mode{RunMode::Realtime};
};

} // namespace archerfish::scenario
