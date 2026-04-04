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
};

enum class MixingMode {
    None,
    Additive
};

struct RepeatSpec {
    int count{1};
    double interval_sec{0.0};
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
};

struct ScenarioEvent {
    std::string target_device;
    double time_sec{0.0};
    std::string type;  // "retune", "gain_change", "marker", "burst"
    nlohmann::json payload;
};

struct ReportingConfig {
    bool save_plan{false};
    bool save_metrics{false};
};

struct Scenario {
    Metadata metadata;
    std::vector<DeviceDef> devices;
    std::vector<WaveformDef> waveforms;
    std::vector<EmitterDef> emitters;
    std::vector<ScenarioEvent> events;
    ReportingConfig reporting;
};

} // namespace archerfish::scenario
