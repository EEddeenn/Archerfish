#include "archerfish/scenario/validator.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace archerfish::scenario {

namespace {

using common::Error;
using common::ErrorCategory;
using common::ErrorList;

static const std::unordered_set<std::string> kValidWaveformTypes = {
    "cw", "chirp", "noise", "qpsk", "bpsk", "8psk",
    "qam16", "qam64", "multi_tone", "file",
    "pulse", "ask", "fsk", "am", "fm", "pm"};

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
    if (!kValidWaveformTypes.count(wf.type)) {
        result.errors.push_back({ErrorCategory::Validation,
                                 "V006_INVALID_WAVEFORM_TYPE",
                                 "Unknown waveform type: '" + wf.type + "'"});
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
                bool overlaps = a.start_after_sec < b.start_after_sec + b.duration_sec &&
                                b.start_after_sec < a.start_after_sec + a.duration_sec;
                if (overlaps) {
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

    return result;
}

} // namespace archerfish::scenario
