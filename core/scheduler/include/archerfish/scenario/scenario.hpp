#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/common/error.hpp"
#include "archerfish/dsp/waveform_type.hpp"

namespace archerfish::scenario {

struct Metadata {
    std::string name;
    std::optional<std::string> description;
    std::optional<std::string> version;
};

struct RfSettings {
    double freq_hz{0.0};
    double rate_sps{0.0};
    double gain_db{0.0};
    std::optional<double> bandwidth_hz;
    std::optional<std::string> antenna;

    RfSettings() = default;
    RfSettings(double f, double r, double g,
               std::optional<double> bw = std::nullopt,
               std::optional<std::string> ant = std::nullopt)
        : freq_hz(f), rate_sps(r), gain_db(g), bandwidth_hz(std::move(bw)), antenna(std::move(ant)) {}

    [[nodiscard]] common::ErrorList validate() const;
};

struct DeviceDef {
    std::string id;
    std::optional<uint32_t> channel;
    RfSettings rf;
};

struct WaveformDef {
    std::optional<std::string> id;
    dsp::WaveformType type{dsp::WaveformType::Unknown};
    nlohmann::json params;
    std::optional<double> target_power_dbm{};
};

struct ImpairmentSettings {
    std::optional<double> cfo_hz;
    std::optional<double> phase_offset_rad;
    std::optional<double> iq_gain_imbalance_db;
    std::optional<double> iq_phase_imbalance_rad;
    std::optional<double> dc_offset_i;
    std::optional<double> dc_offset_q;
    std::optional<double> awgn_power;
    std::optional<double> amplitude_ripple_db;
    std::optional<double> amplitude_ripple_freq_hz;
    std::optional<double> delay_sec;
    std::optional<double> burst_dropout_rate;
    std::optional<double> burst_dropout_mean_burst_sec;
    std::optional<double> phase_noise_bandwidth_hz;
    std::optional<double> phase_noise_magnitude_rad;
    std::optional<std::string> phase_noise_psd_shape;
    std::optional<double> multipath_delay_samples;
    std::optional<double> multipath_amplitude;
    std::optional<double> fading_doppler_hz;
    std::optional<std::string> fading_type;
    std::optional<double> fading_k_factor;
    std::optional<std::string> pa_model;
    std::optional<double> pa_saturation;
    std::optional<double> pa_smoothness;
    std::optional<double> pa_phase_shift;
};

enum class MixingMode {
    None,
    Additive
};

struct RepeatSpec {
    int count{1};
    double interval_sec{0.0};
};

struct ChannelDef {
    std::string id;
    std::string device;
    uint32_t index{0};
    RfSettings rf;
};

struct SyncGroup {
    std::string id;
    std::vector<std::string> channels;
    std::string mode; // "coherent" | "independent"
};

struct EmitterDef {
    std::string id;
    std::string device;
    uint32_t channel{0};
    double start_after_sec{0.0};
    double duration_sec{0.0};
    std::optional<WaveformDef> waveform;
    std::optional<std::string> waveform_ref;
    std::optional<ImpairmentSettings> impairments;
    MixingMode mixing{MixingMode::None};
    std::optional<RepeatSpec> repeat;
    std::optional<std::string> channel_id;
};

struct ScenarioEvent {
    std::string target_device;
    double time_sec{0.0};
    std::string type;  // "retune", "gain_change", "marker", "waveform_switch", "impairment_change"
    nlohmann::json payload;
};

enum class RunMode {
    Realtime,
    Replay
};

struct ReportingConfig {
    bool save_plan{false};
    bool save_metrics{false};
};

struct RunConfig {
    RunMode mode{RunMode::Realtime};
};

struct Scenario {
    Metadata metadata;
    std::vector<DeviceDef> devices;
    std::vector<WaveformDef> waveforms;
    std::vector<EmitterDef> emitters;
    std::vector<ScenarioEvent> events;
    ReportingConfig reporting;
    std::vector<ChannelDef> channel_defs;
    std::vector<SyncGroup> sync_groups;
    RunConfig run;
};

} // namespace archerfish::scenario
