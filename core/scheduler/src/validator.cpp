#include "archerfish/scenario/validator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
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

constexpr int kMaxRepeatCount = 1024;

std::optional<double> get_number_param(const nlohmann::json& params, const char* key) {
    if (!params.contains(key) || !params.at(key).is_number()) {
        return std::nullopt;
    }
    const double value = params.at(key).get<double>();
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

bool is_uint32_json(const nlohmann::json& value) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return false;
    }
    if (value.is_number_integer()) {
        const auto parsed = value.get<std::int64_t>();
        return parsed >= 0 &&
               parsed <= static_cast<std::int64_t>(std::numeric_limits<uint32_t>::max());
    }
    return value.get<std::uint64_t>() <= std::numeric_limits<uint32_t>::max();
}

bool has_string_payload(const ScenarioEvent& evt, const char* key) {
    return evt.payload.contains(key) && evt.payload.at(key).is_string();
}

bool has_non_empty_string_payload(const ScenarioEvent& evt, const char* key) {
    return has_string_payload(evt, key) && !evt.payload.at(key).get<std::string>().empty();
}

std::string channel_binding_key(const std::string& device, uint32_t channel) {
    return device + ":" + std::to_string(channel);
}

std::unordered_set<std::string> effective_channel_bindings(const Scenario& scenario) {
    std::unordered_set<std::string> bindings;
    if (!scenario.channel_defs.empty()) {
        for (const auto& ch : scenario.channel_defs) {
            bindings.insert(channel_binding_key(ch.device, ch.index));
        }
        return bindings;
    }
    for (const auto& dev : scenario.devices) {
        bindings.insert(channel_binding_key(dev.id, dev.channel.value_or(0)));
    }
    return bindings;
}

const WaveformDef* waveform_for_emitter(
    const EmitterDef& emitter,
    const std::unordered_map<std::string, const WaveformDef*>& waveform_map) {
    if (emitter.waveform.has_value()) {
        return &*emitter.waveform;
    }
    if (emitter.waveform_ref.has_value()) {
        auto it = waveform_map.find(*emitter.waveform_ref);
        if (it != waveform_map.end()) {
            return it->second;
        }
    }
    return nullptr;
}

double emitter_amplitude(
    const EmitterDef& emitter,
    const std::unordered_map<std::string, const WaveformDef*>& waveform_map) {
    const auto* waveform = waveform_for_emitter(emitter, waveform_map);
    if (waveform == nullptr) {
        return 0.0;
    }
    return get_number_param(waveform->params, "amplitude").value_or(0.0);
}

void check_unique_ids(const Scenario& scenario, ValidationResult& result) {
    std::unordered_set<std::string> device_ids;
    for (const auto& dev : scenario.devices) {
        if (dev.id.empty()) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V010_EMPTY_DEVICE_ID",
                                     "Device id must be set"});
        } else if (!device_ids.insert(dev.id).second) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V010_DUPLICATE_DEVICE_ID",
                                     "Duplicate device id: '" + dev.id + "'"});
        }
    }

    std::unordered_set<std::string> emitter_ids;
    for (const auto& em : scenario.emitters) {
        if (em.id.empty()) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V011_EMPTY_EMITTER_ID",
                                     "Emitter id must be set"});
        } else if (!emitter_ids.insert(em.id).second) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V011_DUPLICATE_EMITTER_ID",
                                     "Duplicate emitter id: '" + em.id + "'"});
        }
    }

    std::unordered_set<std::string> waveform_ids;
    for (const auto& wf : scenario.waveforms) {
        if (wf.id.has_value()) {
            if (wf.id->empty()) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V012_EMPTY_WAVEFORM_ID",
                                         "Waveform id must be non-empty when provided"});
            } else if (!waveform_ids.insert(*wf.id).second) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V012_DUPLICATE_WAVEFORM_ID",
                                         "Duplicate waveform id: '" + *wf.id + "'"});
            }
        }
    }
}

void check_not_empty(const Scenario& scenario, ValidationResult& result) {
    if (scenario.metadata.name.empty()) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V013_MISSING_METADATA_NAME",
                                 "Scenario metadata.name must be set"});
    }
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
    if (!std::isfinite(device.rf.freq_hz) || device.rf.freq_hz <= 0.0) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V007_INVALID_FREQ",
                                 "Device '" + device.id +
                                     "': freq_hz must be > 0"});
    }
    if (!std::isfinite(device.rf.rate_sps) || device.rf.rate_sps <= 0.0) {
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
        auto amp_value = get_number_param(wf.params, "amplitude");
        if (!amp_value.has_value() || !std::isfinite(*amp_value) || *amp_value <= 0.0 || *amp_value > 1.0) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V001_INVALID_AMPLITUDE",
                                     "Waveform amplitude must be a finite number in (0, 1]"});
        } else if (*amp_value > 0.9) {
            result.warnings.push_back({ErrorCategory::QualityWarning,
                                       "V001_AMPLITUDE_CLIPPING_RISK",
                                       "Waveform amplitude " + std::to_string(*amp_value) +
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
    if (!std::isfinite(emitter.duration_sec) || emitter.duration_sec <= 0.0) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V002_INVALID_DURATION",
                                 "Emitter '" + emitter.id +
                                     "': duration_sec must be > 0"});
    }
    if (!std::isfinite(emitter.start_after_sec) || emitter.start_after_sec < 0.0) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V002_INVALID_START",
                                 "Emitter '" + emitter.id +
                                     "': start_after_sec must be finite and >= 0"});
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

    if (emitter.waveform.has_value() && emitter.waveform_ref.has_value()) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V005_AMBIGUOUS_WAVEFORM",
                                 "Emitter '" + emitter.id +
                                     "' must not set both inline waveform and waveform_ref"});
    } else if (!emitter.waveform.has_value() && !emitter.waveform_ref.has_value()) {
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

ChannelKey channel_key_for_emitter(
    const EmitterDef& emitter,
    const std::unordered_map<std::string, const ChannelDef*>& channel_map) {
    if (emitter.channel_id.has_value()) {
        auto it = channel_map.find(*emitter.channel_id);
        if (it != channel_map.end()) {
            return {it->second->device, it->second->index};
        }
    }
    return {emitter.device, emitter.channel};
}

std::vector<EmitterWindow> get_emitter_windows(const EmitterDef& em) {
    std::vector<EmitterWindow> windows;
    int count = em.repeat.has_value() ? em.repeat->count : 1;
    double interval = em.repeat.has_value() ? em.repeat->interval_sec : 0.0;
    if (count < 1 || count > kMaxRepeatCount ||
        !std::isfinite(interval) || interval < 0.0 ||
        (count > 1 && interval <= 0.0) ||
        !std::isfinite(em.start_after_sec) || !std::isfinite(em.duration_sec)) {
        return windows;
    }
    for (int i = 0; i < count; ++i) {
        double s = em.start_after_sec + static_cast<double>(i) * interval;
        if (!std::isfinite(s)) {
            return {};
        }
        windows.push_back({s, s + em.duration_sec});
    }
    return windows;
}

void check_overlaps(const Scenario& scenario,
                    const std::unordered_map<std::string, const WaveformDef*>& waveform_map,
                    ValidationResult& result) {
    std::unordered_map<ChannelKey, std::vector<size_t>, ChannelKeyHash> by_channel;
    std::unordered_map<std::string, const ChannelDef*> channel_map;

    for (const auto& ch : scenario.channel_defs) {
        channel_map[ch.id] = &ch;
    }

    for (size_t i = 0; i < scenario.emitters.size(); ++i) {
        const auto& em = scenario.emitters[i];
        by_channel[channel_key_for_emitter(em, channel_map)].push_back(i);
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
                        const double peak_a = emitter_amplitude(a, waveform_map);
                        const double peak_b = emitter_amplitude(b, waveform_map);
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
    const auto channel_bindings = effective_channel_bindings(scenario);
    for (const auto& evt : scenario.events) {
        if (!std::isfinite(evt.time_sec) || evt.time_sec < 0.0) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V014_INVALID_EVENT_TIME",
                                     "Event time_sec must be finite and >= 0"});
        }
        if (!device_ids.count(evt.target_device)) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V013_EVENT_UNKNOWN_DEVICE",
                                     "Event targets unknown device '" + evt.target_device + "'"});
        }
        if (evt.type != "retune" && evt.type != "gain_change" && evt.type != "marker" &&
            evt.type != "waveform_switch" && evt.type != "impairment_change") {
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
            } else if (!evt.payload.at("freq_hz").is_number() ||
                       !std::isfinite(evt.payload.at("freq_hz").get<double>()) ||
                       evt.payload.at("freq_hz").get<double>() <= 0.0) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V015_RETUNE_INVALID_FREQ",
                                         "Retune event freq_hz must be a finite number > 0"});
            }
            if (evt.payload.contains("channel") && !is_uint32_json(evt.payload.at("channel"))) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V015_RETUNE_INVALID_CHANNEL",
                                         "Retune event channel must be an unsigned 32-bit integer"});
            } else if (evt.payload.contains("channel") && device_ids.count(evt.target_device)) {
                uint32_t channel = evt.payload.at("channel").is_number_integer()
                                       ? static_cast<uint32_t>(evt.payload.at("channel").get<std::int64_t>())
                                       : static_cast<uint32_t>(evt.payload.at("channel").get<std::uint64_t>());
                if (!channel_bindings.count(channel_binding_key(evt.target_device, channel))) {
                    result.errors.push_back({ErrorCategory::Validation,
                                             "V015_RETUNE_UNKNOWN_CHANNEL",
                                             "Retune event channel " + std::to_string(channel) +
                                                 " is not bound to device '" + evt.target_device + "'"});
                }
            }
        }
        if (evt.type == "gain_change") {
            if (!evt.payload.contains("gain_db")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V016_GAIN_MISSING_DB",
                                         "Gain change event for device '" + evt.target_device +
                                             "' missing gain_db in payload"});
            } else if (!evt.payload.at("gain_db").is_number() ||
                       !std::isfinite(evt.payload.at("gain_db").get<double>())) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V016_GAIN_INVALID_DB",
                                         "Gain change event gain_db must be a finite number"});
            }
            if (evt.payload.contains("channel") && !is_uint32_json(evt.payload.at("channel"))) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V016_GAIN_INVALID_CHANNEL",
                                         "Gain change event channel must be an unsigned 32-bit integer"});
            } else if (evt.payload.contains("channel") && device_ids.count(evt.target_device)) {
                uint32_t channel = evt.payload.at("channel").is_number_integer()
                                       ? static_cast<uint32_t>(evt.payload.at("channel").get<std::int64_t>())
                                       : static_cast<uint32_t>(evt.payload.at("channel").get<std::uint64_t>());
                if (!channel_bindings.count(channel_binding_key(evt.target_device, channel))) {
                    result.errors.push_back({ErrorCategory::Validation,
                                             "V016_GAIN_UNKNOWN_CHANNEL",
                                             "Gain change event channel " + std::to_string(channel) +
                                                 " is not bound to device '" + evt.target_device + "'"});
                }
            }
        }
        if (evt.type == "marker") {
            if (evt.payload.contains("name") && !evt.payload.at("name").is_string()) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V021_MARKER_INVALID_NAME",
                                         "Marker event name must be a string when provided"});
            } else if (evt.payload.contains("name") && !has_non_empty_string_payload(evt, "name")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V021_MARKER_EMPTY_NAME",
                                         "Marker event name must be non-empty when provided"});
            }
        }
        if (evt.type == "waveform_switch") {
            if (!evt.payload.contains("emitter_id")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V019_WAVEFORM_SWITCH_MISSING_EMITTER",
                                         "Waveform switch event for device '" + evt.target_device +
                                             "' missing emitter_id in payload"});
            } else if (!has_string_payload(evt, "emitter_id")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V019_WAVEFORM_SWITCH_INVALID_EMITTER",
                                         "Waveform switch event emitter_id must be a string"});
            } else if (!has_non_empty_string_payload(evt, "emitter_id")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V019_WAVEFORM_SWITCH_EMPTY_EMITTER",
                                         "Waveform switch event emitter_id must be non-empty"});
            }
            if (!evt.payload.contains("new_waveform")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V019_WAVEFORM_SWITCH_MISSING_WAVEFORM",
                                         "Waveform switch event for device '" + evt.target_device +
                                             "' missing new_waveform in payload"});
            } else if (!has_string_payload(evt, "new_waveform")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V019_WAVEFORM_SWITCH_INVALID_WAVEFORM",
                                         "Waveform switch event new_waveform must be a string"});
            } else if (!has_non_empty_string_payload(evt, "new_waveform")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V019_WAVEFORM_SWITCH_EMPTY_WAVEFORM",
                                         "Waveform switch event new_waveform must be non-empty"});
            }
        }
        if (evt.type == "impairment_change") {
            if (!evt.payload.contains("emitter_id")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V020_IMPAIRMENT_CHANGE_MISSING_EMITTER",
                                         "Impairment change event for device '" + evt.target_device +
                                             "' missing emitter_id in payload"});
            } else if (!has_string_payload(evt, "emitter_id")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V020_IMPAIRMENT_CHANGE_INVALID_EMITTER",
                                         "Impairment change event emitter_id must be a string"});
            } else if (!has_non_empty_string_payload(evt, "emitter_id")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V020_IMPAIRMENT_CHANGE_EMPTY_EMITTER",
                                         "Impairment change event emitter_id must be non-empty"});
            }
            if (!evt.payload.contains("impairment")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V020_IMPAIRMENT_CHANGE_MISSING_IMPAIRMENT",
                                         "Impairment change event for device '" + evt.target_device +
                                             "' missing impairment name in payload"});
            } else if (!has_string_payload(evt, "impairment")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V020_IMPAIRMENT_CHANGE_INVALID_IMPAIRMENT",
                                         "Impairment change event impairment must be a string"});
            } else if (!has_non_empty_string_payload(evt, "impairment")) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V020_IMPAIRMENT_CHANGE_EMPTY_IMPAIRMENT",
                                         "Impairment change event impairment must be non-empty"});
            }
            if (evt.payload.contains("enabled") && !evt.payload.at("enabled").is_boolean()) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V020_IMPAIRMENT_CHANGE_INVALID_ENABLED",
                                         "Impairment change event enabled must be a boolean when provided"});
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
            if (em.repeat->count > kMaxRepeatCount) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V017_INVALID_REPEAT_COUNT",
                                         "Emitter '" + em.id + "': repeat count must be <= " +
                                             std::to_string(kMaxRepeatCount)});
            }
            if (em.repeat->count > 1 && em.repeat->interval_sec <= 0.0) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V018_INVALID_REPEAT_INTERVAL",
                                         "Emitter '" + em.id +
                                             "': repeat interval_sec must be > 0 when count > 1"});
            }
            if (!std::isfinite(em.repeat->interval_sec) || em.repeat->interval_sec < 0.0) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V018_INVALID_REPEAT_INTERVAL",
                                         "Emitter '" + em.id +
                                             "': repeat interval_sec must be finite and >= 0"});
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

    std::unordered_set<std::string> seen_channel_ids;
    std::unordered_set<std::string> device_index_pairs;

    for (const auto& ch : scenario.channel_defs) {
        if (!std::isfinite(ch.rf.freq_hz) || ch.rf.freq_hz <= 0.0) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V027_INVALID_CHANNEL_FREQ",
                                     "Channel '" + ch.id + "': rf.freq_hz must be > 0"});
        }
        if (!std::isfinite(ch.rf.rate_sps) || ch.rf.rate_sps <= 0.0) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V027_INVALID_CHANNEL_RATE",
                                     "Channel '" + ch.id + "': rf.rate_sps must be > 0"});
        }
        if (ch.id.empty()) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V020_EMPTY_CHANNEL_ID",
                                     "Channel id must be set"});
        } else if (!seen_channel_ids.insert(ch.id).second) {
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

    std::unordered_set<std::string> seen_sync_group_ids;
    for (const auto& sg : scenario.sync_groups) {
        if (sg.id.empty()) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V028_SYNC_MISSING_ID",
                                     "Sync group id must be set"});
        } else if (!seen_sync_group_ids.insert(sg.id).second) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V028_DUPLICATE_SYNC_ID",
                                     "Duplicate sync group id: '" + sg.id + "'"});
        }
        if (sg.channels.empty()) {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V029_SYNC_NO_CHANNELS",
                                     "Sync group '" + sg.id + "' must reference at least one channel"});
        }
        if (sg.mode != "coherent" && sg.mode != "independent") {
            result.errors.push_back({ErrorCategory::Validation,
                                     "V030_SYNC_INVALID_MODE",
                                     "Sync group '" + sg.id + "' mode must be 'coherent' or 'independent'"});
        }

        std::unordered_set<std::string> seen_sync_channels;
        for (const auto& cid : sg.channels) {
            if (!channel_ids.count(cid)) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V024_SYNC_UNKNOWN_CHANNEL",
                                         "Sync group '" + sg.id + "' references unknown channel '" + cid + "'"});
            }
            if (!seen_sync_channels.insert(cid).second) {
                result.errors.push_back({ErrorCategory::Validation,
                                         "V031_SYNC_DUPLICATE_CHANNEL",
                                         "Sync group '" + sg.id + "' references channel '" + cid + "' more than once"});
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

void check_high_power(const Scenario& scenario,
                      const std::unordered_map<std::string, const WaveformDef*>& waveform_map,
                      ValidationResult& result) {
    for (const auto& em : scenario.emitters) {
        const auto* waveform = waveform_for_emitter(em, waveform_map);
        if (waveform == nullptr) continue;
        double gain = 0.0;

        for (const auto& dev : scenario.devices) {
            if (dev.id == em.device) {
                gain = dev.rf.gain_db;
                break;
            }
        }
        const double amplitude = get_number_param(waveform->params, "amplitude").value_or(0.0);
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
            amplitude = get_number_param(wf.params, "amplitude").value_or(0.0);
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

void check_safety(const Scenario& scenario,
                  const std::unordered_map<std::string, const WaveformDef*>& waveform_map,
                  ValidationResult& result) {
    auto profile = common::get_lab_safe_profile();
    for (const auto& dev : scenario.devices) {
        double amplitude = 0.0;
        for (const auto& em : scenario.emitters) {
            if (em.device == dev.id) {
                amplitude = std::max(amplitude, emitter_amplitude(em, waveform_map));
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
    std::unordered_map<std::string, const WaveformDef*> waveform_map;
    for (const auto& wf : scenario.waveforms) {
        if (wf.id.has_value()) {
            waveform_ids.insert(*wf.id);
            waveform_map[*wf.id] = &wf;
        }
        validate_waveform(wf, result);
    }

    for (const auto& em : scenario.emitters) {
        check_emitter(em, device_ids, device_map, waveform_ids, result);
    }

    check_repeat(scenario, result);
    check_overlaps(scenario, waveform_map, result);
    check_events(scenario, device_ids, result);
    check_channels(scenario, device_ids, result);
    check_regulatory_bands(scenario, device_map, result);
    check_high_power(scenario, waveform_map, result);
    check_safety(scenario, waveform_map, result);

    return result;
}

} // namespace archerfish::scenario
