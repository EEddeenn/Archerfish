#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/common/error.hpp"

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

    [[nodiscard]] common::ErrorList validate() const;
};

struct DeviceDef {
    std::string id;
    std::optional<uint32_t> channel;
    RfSettings rf;
};

struct WaveformDef {
    std::optional<std::string> id;
    std::string type;
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

struct EmitterDef {
    std::string id;
    std::string device;
    uint32_t channel{0};
    double start_after_sec{0.0};
    double duration_sec{0.0};
    std::optional<WaveformDef> waveform;
    std::optional<std::string> waveform_ref;
    std::optional<ImpairmentSettings> impairments;
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
    ReportingConfig reporting;
};

} // namespace archerfish::scenario
