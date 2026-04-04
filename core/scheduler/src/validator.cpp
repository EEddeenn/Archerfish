#include "archerfish/scenario/validator.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>

#include "archerfish/dsp/waveform_type.hpp"
#include "archerfish/common/regulatory.hpp"
#include "archerfish/common/safety_profile.hpp"

namespace archerfish::scenario {

namespace {

using common::Error;
using common::ErrorCategory;
using common::ErrorList;

void check_unique_ids(const Scenario& scenario, ValidationResult& result) {
    std::unordered_set<std::string> device_ids;
    for (const auto& dev : scenario.devices) {
        if (!device_ids.insert(dev.id).second) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V010_DUPLICATE_DEVICE_ID",
                                     "Duplicate device id: '" + dev.id + "'"});
        }
    }

    std::unordered_set<std::string> emitter_ids;
    for (const auto& em : scenario.emitters) {
        if (!emitter_ids.insert(em.id).second) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V011_DUPLICATE_EMITTER_ID",
                                     "Duplicate emitter id: '" + em.id + "'"});
        }
    }

    std::unordered_set<std::string> waveform_ids;
    for (const auto& wf : scenario.waveforms) {
        if (wf.id.has_value()) {
            if (!waveform_ids.insert(*wf.id).second) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V012_DUPLICATE_WAVEFORM_ID",
                                         "Duplicate waveform id: '" + *wf.id + "'"});
            }
        }
    }
}

void check_not_empty(const Scenario& scenario, ValidationResult& result) {
    if (scenario.devices.empty()) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V008_NO_DEVICES",
                                 "Scenario must define at least one device"});
    }
    if (scenario.emitters.empty()) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V009_NO_EMITTERS",
                                 "Scenario must define at least one emitter"});
    }
}

void check_rf_settings(const DeviceDef& device, ValidationResult& result) {
    if (device.rf.freq_hz <= 0.0) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V007_INVALID_FREQ",
                                 "Device '" + device.id +
                                     "': freq_hz must be > 0"});
    }
    if (device.rf.rate_sps <= 0.0) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V007_INVALID_RATE",
                                 "Device '" + device.id +
                                     "': rate_sps must be > 0"});
    }
}

void check_waveform_type(const WaveformDef& wf, ValidationResult& result) {
    auto name = dsp::to_string(wf.type);
    if (name == "unknown") {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V006_INVALID_WAVEFORM_TYPE",
                                 "Unknown waveform type"});
    }
}

void check_amplitude(const WaveformDef& wf, ValidationResult& result) {
    if (wf.params.contains("amplitude")) {
        double amp = wf.params["amplitude"].get<double>();
        if (amp <= 0.0 || amp > 1.0) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V001_INVALID_AMPLITUDE",
                                     "Waveform amplitude must be in (0, 1], got " +
                                         std::to_string(amp)});
        } else if (amp > 0.9) {
            result.warnings.push_back({ErrorCategory::QualityWarning,
                                       "V001_AMPLITUDE_CLIPPING_RISK",
                                       "Waveform amplitude " + std::to_string(amp) +
                                           " is near clipping threshold (>0.9)"});
        }
    }
}

void validate_waveform(const WaveformDef& wf, ValidationResult& result) {
    check_waveform_type(wf, result);
    check_amplitude(wf, result);
}

void check_emitter(const EmitterDef& emitter,
                   const std::unordered_set<std::string>& device_ids,
                   const std::unordered_map<std::string, const DeviceDef*>& device_map,
                   const std::unordered_set<std::string>& waveform_ids,
                   ValidationResult& result) {
    if (emitter.duration_sec <= 0.0) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V002_INVALID_DURATION",
                                 "Emitter '" + emitter.id +
                                     "': duration_sec must be > 0"});
    }

    if (!device_ids.count(emitter.device)) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V003_DANGLING_DEVICE_REF",
                                 "Emitter '" + emitter.id +
                                     "' references unknown device '" +
                                     emitter.device + "'"});
    } else {
        auto it = device_map.find(emitter.device);
        if (it != device_map.end()) {
            const auto& dev = *it->second;
            if (dev.channel.has_value() && emitter.channel != *dev.channel) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V004_CHANNEL_MISMATCH",
                                         "Emitter '" + emitter.id +
                                             "' channel " +
                                             std::to_string(emitter.channel) +
                                             " does not match device '" + dev.id +
                                             "' channel " +
                                             std::to_string(*dev.channel)});
            }
        }
    }

    if (!emitter.waveform.has_value() && !emitter.waveform_ref.has_value()) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V005_NO_WAVEFORM",
                                 "Emitter '" + emitter.id +
                                     "' must have either inline waveform or waveform_ref"});
    }

    if (emitter.waveform_ref.has_value()) {
        if (!waveform_ids.count(*emitter.waveform_ref)) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V005_DANGLING_WAVEFORM_REF",
                                     "Emitter '" + emitter.id +
                                         "' references unknown waveform '" +
                                         *emitter.waveform_ref + "'"});
        }
    }

    if (emitter.waveform.has_value()) {
        validate_waveform(*emitter.waveform, result);
    }
}

struct ChannelKey {
    std::string device;
    uint32_t channel;
    bool operator==(const ChannelKey& o) const { return device == o.device && channel == o.channel; }
};

struct ChannelKeyHash {
    size_t operator()(const ChannelKey& k) const {
        size_t h = std::hash<std::string>{}(k.device);
        h ^= std::hash<uint32_t>{}(k.channel) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

struct EmitterWindow {
    double start;
    double end;
};

std::vector<EmitterWindow> get_emitter_windows(const EmitterDef& em) {
    std::vector<EmitterWindow> windows;
    int count = em.repeat.has_value() ? em.repeat->count : 1;
    double interval = em.repeat.has_value() ? em.repeat->interval_sec : 0.0;
    for (int i = 0; i < count; ++i) {
        double s = em.start_after_sec + static_cast<double>(i) * interval;
        windows.push_back({s, s + em.duration_sec});
    }
    return windows;
}

void check_overlaps(const Scenario& scenario, ValidationResult& result) {
    std::unordered_map<ChannelKey, std::vector<size_t>, ChannelKeyHash> by_channel;

    for (size_t i = 0; i < scenario.emitters.size(); ++i) {
        const auto& em = scenario.emitters[i];
        by_channel[{em.device, em.channel}].push_back(i);
    }

    for (const auto& [key, indices] : by_channel) {
        for (size_t i = 0; i < indices.size(); ++i) {
            for (size_t j = i + 1; j < indices.size(); ++j) {
                const auto& a = scenario.emitters[indices[i]];
                const auto& b = scenario.emitters[indices[j]];
                auto a_windows = get_emitter_windows(a);
                auto b_windows = get_emitter_windows(b);
                bool overlaps = false;
                for (const auto& aw : a_windows) {
                    for (const auto& bw : b_windows) {
                        if (aw.start < bw.end && bw.start < aw.end) {
                            overlaps = true;
                            break;
                        }
                    }
                    if (overlaps) break;
                }
                if (overlaps) {
                    bool mixing_allowed = (a.mixing == MixingMode::Additive) &&
                                          (b.mixing == MixingMode::Additive);
                    if (mixing_allowed) {
                        double peak_a = 0.0, peak_b = 0.0;
                        if (a.waveform.has_value() && a.waveform->params.contains("amplitude"))
                            peak_a = a.waveform->params["amplitude"].get<double>();
                        if (b.waveform.has_value() && b.waveform->params.contains("amplitude"))
                            peak_b = b.waveform->params["amplitude"].get<double>();
                        if (peak_a + peak_b > 1.0) {
                            result.warnings.push_back(
                                {ErrorCategory::QualityWarning,
                                 "W_MIX_HEADROOM",
                                 "Emitters '" + a.id + "' and '" + b.id +
                                     "' additive mix on device '" + key.device +
                                     "' channel " + std::to_string(key.channel) +
                                     ": estimated peak sum " +
                                     std::to_string(peak_a + peak_b) + " exceeds 1.0"});
                        }
                    } else {
                        result.errors.push_back(
                            {ErrorCategory::Validation,
                             "V002_OVERLAPPING_EMITTERS",
                             "Emitters '" + a.id + "' and '" + b.id +
                                 "' overlap on device '" + key.device +
                                 "' channel " + std::to_string(key.channel)});
                    }
                }
            }
        }
    }
}

void check_events(const Scenario& scenario, const std::unordered_set<std::string>& device_ids,
                  ValidationResult& result) {
    for (const auto& evt : scenario.events) {
        if (!device_ids.count(evt.target_device)) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V013_EVENT_UNKNOWN_DEVICE",
                                     "Event targets unknown device '" + evt.target_device + "'"});
        }
        if (evt.type != "retune" && evt.type != "gain_change" && evt.type != "marker" &&
            evt.type != "burst" && evt.type != "waveform_switch" && evt.type != "impairment_change") {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V014_INVALID_EVENT_TYPE",
                                     "Unknown event type '" + evt.type + "'"});
        }
        if (evt.type == "retune") {
            if (!evt.payload.contains("freq_hz")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V015_RETUNE_MISSING_FREQ",
                                         "Retune event for device '" + evt.target_device +
                                             "' missing freq_hz in payload"});
            }
        }
        if (evt.type == "gain_change") {
            if (!evt.payload.contains("gain_db")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V016_GAIN_MISSING_DB",
                                         "Gain change event for device '" + evt.target_device +
                                             "' missing gain_db in payload"});
            }
        }
        if (evt.type == "waveform_switch") {
            if (!evt.payload.contains("emitter_id")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V019_WAVEFORM_SWITCH_MISSING_EMITTER",
                                         "Waveform switch event for device '" + evt.target_device +
                                             "' missing emitter_id in payload"});
            }
            if (!evt.payload.contains("new_waveform")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V019_WAVEFORM_SWITCH_MISSING_WAVEFORM",
                                         "Waveform switch event for device '" + evt.target_device +
                                             "' missing new_waveform in payload"});
            }
        }
        if (evt.type == "impairment_change") {
            if (!evt.payload.contains("emitter_id")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V020_IMPAIRMENT_CHANGE_MISSING_EMITTER",
                                         "Impairment change event for device '" + evt.target_device +
                                             "' missing emitter_id in payload"});
            }
            if (!evt.payload.contains("impairment")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V020_IMPAIRMENT_CHANGE_MISSING_IMPAIRMENT",
                                         "Impairment change event for device '" + evt.target_device +
                                             "' missing impairment name in payload"});
            }
        }
    }
}

void check_repeat(const Scenario& scenario, ValidationResult& result) {
    for (const auto& em : scenario.emitters) {
        if (em.repeat.has_value()) {
            if (em.repeat->count < 1) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V017_INVALID_REPEAT_COUNT",
                                         "Emitter '" + em.id + "': repeat count must be >= 1"});
            }
            if (em.repeat->count > 1 && em.repeat->interval_sec <= 0.0) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V018_INVALID_REPEAT_INTERVAL",
                                         "Emitter '" + em.id +
                                             "': repeat interval_sec must be > 0 when count > 1"});
            }
        }
    }
}

void check_channels(const Scenario& scenario,
                    const std::unordered_set<std::string>& device_ids,
                    ValidationResult& result) {
    // Always check channel_id references, even when channel_defs is empty
    std::unordered_set<std::string> channel_ids;
    for (const auto& ch : scenario.channel_defs) {
        channel_ids.insert(ch.id);
    }
    for (const auto& em : scenario.emitters) {
        if (em.channel_id.has_value()) {
            if (!channel_ids.count(*em.channel_id)) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V023_EMITTER_UNKNOWN_CHANNEL",
                                         "Emitter '" + em.id + "' references unknown channel_id '" + *em.channel_id + "'"});
            }
        }
    }

    if (scenario.channel_defs.empty()) return;

    std::unordered_set<std::string> seen_channel_ids;
    std::unordered_set<std::string> device_index_pairs;

    for (const auto& ch : scenario.channel_defs) {
        if (!seen_channel_ids.insert(ch.id).second) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V020_DUPLICATE_CHANNEL_ID",
                                     "Duplicate channel id: '" + ch.id + "'"});
        }
        if (!device_ids.count(ch.device)) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V021_CHANNEL_UNKNOWN_DEVICE",
                                     "Channel '" + ch.id + "' references unknown device '" + ch.device + "'"});
        }
        std::string dev_idx_key = ch.device + ":" + std::to_string(ch.index);
        if (!device_index_pairs.insert(dev_idx_key).second) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V022_DUPLICATE_DEVICE_INDEX",
                                     "Duplicate (device, index) pair for channel '" + ch.id + "': " + dev_idx_key});
        }
    }

    for (const auto& sg : scenario.sync_groups) {
        for (const auto& cid : sg.channels) {
            if (!channel_ids.count(cid)) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V024_SYNC_UNKNOWN_CHANNEL",
                                         "Sync group '" + sg.id + "' references unknown channel '" + cid + "'"});
            }
        }
        if (sg.mode == "coherent" && sg.channels.size() >= 2) {
            std::string first_device;
            double first_rate = 0.0;
            bool first = true;
            for (const auto& cid : sg.channels) {
                auto it = std::find_if(scenario.channel_defs.begin(), scenario.channel_defs.end(),
                                       [&](const ChannelDef& c) { return c.id == cid; });
                if (it != scenario.channel_defs.end()) {
                    if (first) {
                        first_device = it->device;
                        first_rate = it->rf.rate_sps;
                        first = false;
                    } else {
                        if (it->device != first_device) {
                            result.errors.push_back({ErrorCategory::Validation,
                                                     "V025_COHERENT_DIFFERENT_DEVICES",
                                                     "Sync group '" + sg.id + "': coherent mode requires all channels on the same device"});
                            break;
                        }
                        if (it->rf.rate_sps != first_rate) {
                            result.errors.push_back({ErrorCategory::Validation,
                                                     "V026_COHERENT_DIFFERENT_RATES",
                                                     "Sync group '" + sg.id + "': coherent mode requires all channels to have the same sample rate"});
                            break;
                        }
                    }
                }
            }
        }
    }
}

void check_regulatory_bands(const Scenario& scenario,
                            const std::unordered_map<std::string, const DeviceDef*>& device_map,
                            ValidationResult& result) {
    for (const auto& dev : scenario.devices) {
        double bw = dev.rf.bandwidth_hz.value_or(0.0);
        auto warnings = common::check_regulatory(dev.rf.freq_hz, bw);
        for (auto& w : warnings) {
            result.warnings.push_back(std::move(w));
        }
    }
    for (const auto& em : scenario.emitters) {
        if (em.waveform.has_value() && em.waveform->target_power_dbm.has_value()) {
            auto it = device_map.find(em.device);
            if (it != device_map.end()) {
                double bw = it->second->rf.bandwidth_hz.value_or(0.0);
                auto warnings = common::check_regulatory(it->second->rf.freq_hz, bw);
                for (auto& w : warnings) {
                    result.warnings.push_back(std::move(w));
                }
            }
        }
    }
}

void check_high_power(const Scenario& scenario, ValidationResult& result) {
    for (const auto& em : scenario.emitters) {
        if (!em.waveform.has_value()) continue;
        const auto& wf = *em.waveform;
        double gain = 0.0;
        double amplitude = 0.0;

        for (const auto& dev : scenario.devices) {
            if (dev.id == em.device) {
                gain = dev.rf.gain_db;
                break;
            }
        }
        if (wf.params.contains("amplitude")) {
            amplitude = wf.params["amplitude"].get<double>();
        }
        if (gain > 25.0 && amplitude > 0.5) {
            result.warnings.push_back({ErrorCategory::QualityWarning,
                                       "W_HIGH_POWER",
                                       "Emitter '" + em.id + "': high power combination (gain " +
                                           std::to_string(gain) + " dB, amplitude " +
                                           std::to_string(amplitude) + ") may cause device damage or regulatory violation"});
        }
    }
    for (const auto& wf : scenario.waveforms) {
        double amplitude = 0.0;
        if (wf.params.contains("amplitude")) {
            amplitude = wf.params["amplitude"].get<double>();
        }
        if (wf.target_power_dbm.has_value() && amplitude > 0.5) {
            result.warnings.push_back({ErrorCategory::QualityWarning,
                                       "W_HIGH_POWER_TARGET",
                                       "Waveform" + (wf.id.has_value() ? " '" + *wf.id + "'" : "") +
                                           ": target_power_dbm with amplitude " +
                                           std::to_string(amplitude) + " > 0.5 may cause clipping"});
        }
    }
}

void check_safety(const Scenario& scenario, ValidationResult& result) {
    auto profile = common::get_lab_safe_profile();
    for (const auto& dev : scenario.devices) {
        double amplitude = 0.0;
        for (const auto& em : scenario.emitters) {
            if (em.device == dev.id && em.waveform.has_value() &&
                em.waveform->params.contains("amplitude")) {
                amplitude = std::max(amplitude, em.waveform->params["amplitude"].get<double>());
            }
        }
        auto errors = common::check_safety_profile(profile, dev.rf.gain_db, amplitude, dev.rf.freq_hz);
        for (auto& e : errors) {
            result.warnings.push_back(std::move(e));
        }
    }
}

} // namespace

ValidationResult validate(const Scenario& scenario) {
    ValidationResult result;

    check_not_empty(scenario, result);
    check_unique_ids(scenario, result);

    std::unordered_set<std::string> device_ids;
    std::unordered_map<std::string, const DeviceDef*> device_map;
    for (const auto& dev : scenario.devices) {
        device_ids.insert(dev.id);
        device_map[dev.id] = &dev;
        check_rf_settings(dev, result);
    }

    std::unordered_set<std::string> waveform_ids;
    for (const auto& wf : scenario.waveforms) {
        if (wf.id.has_value()) {
            waveform_ids.insert(*wf.id);
        }
        validate_waveform(wf, result);
    }

    for (const auto& em : scenario.emitters) {
        check_emitter(em, device_ids, device_map, waveform_ids, result);
    }

    check_overlaps(scenario, result);
    check_events(scenario, device_ids, result);
    check_repeat(scenario, result);
    check_channels(scenario, device_ids, result);
    check_regulatory_bands(scenario, device_map, result);
    check_high_power(scenario, result);
    check_safety(scenario, result);

    return result;
}

} // namespace archerfish::scenario
