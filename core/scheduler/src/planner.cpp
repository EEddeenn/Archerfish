#include "archerfish/scenario/planner.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace archerfish::scenario {

[[nodiscard]] std::expected<Plan, common::ErrorList> plan(const Scenario& scenario) {
    using common::Error;
    using common::ErrorCategory;
    using common::ErrorList;

    ErrorList errors;

    if (scenario.emitters.empty()) {
        errors.push_back(Error{
            ErrorCategory::Planning,
            "E_PLAN_NO_EMITTERS",
            "Scenario has no emitters; nothing to plan"});
        return std::unexpected(std::move(errors));
    }

    std::unordered_map<std::string, const DeviceDef*> device_map;
    for (const auto& dev : scenario.devices) {
        device_map[dev.id] = &dev;
    }

    std::unordered_map<std::string, const WaveformDef*> waveform_map;
    for (const auto& wf : scenario.waveforms) {
        if (wf.id.has_value()) {
            waveform_map[*wf.id] = &wf;
        }
    }

    Plan result;
    result.normalized_scenario = scenario;

    for (const auto& dev : scenario.devices) {
        ChannelBinding binding;
        binding.device_id = dev.id;
        binding.channel_index = dev.channel.value_or(0);
        binding.rf = dev.rf;
        result.channels.push_back(std::move(binding));
    }

    double max_end_time = 0.0;

    for (const auto& emitter : scenario.emitters) {
        auto dev_it = device_map.find(emitter.device);
        if (dev_it == device_map.end()) {
            errors.push_back(Error{
                ErrorCategory::Planning,
                "E_PLAN_UNKNOWN_DEVICE",
                "Emitter '" + emitter.id + "' references unknown device '" + emitter.device + "'"});
            continue;
        }
        const auto& device = *dev_it->second;

        double start = emitter.start_after_sec;
        double stop = start + emitter.duration_sec;

        TimelineEvent start_event;
        start_event.type = TimelineEventType::EmitterStart;
        start_event.time_sec = start;
        start_event.target_id = emitter.id;
        start_event.payload["device_id"] = emitter.device;
        start_event.payload["channel"] = emitter.channel;
        result.timeline.push_back(std::move(start_event));

        TimelineEvent stop_event;
        stop_event.type = TimelineEventType::EmitterStop;
        stop_event.time_sec = stop;
        stop_event.target_id = emitter.id;
        result.timeline.push_back(std::move(stop_event));

        WaveformDef resolved_waveform;
        if (emitter.waveform.has_value()) {
            resolved_waveform = *emitter.waveform;
        } else if (emitter.waveform_ref.has_value()) {
            auto wf_it = waveform_map.find(*emitter.waveform_ref);
            if (wf_it != waveform_map.end()) {
                resolved_waveform = *wf_it->second;
            } else {
                errors.push_back(Error{
                    ErrorCategory::Planning,
                    "E_PLAN_UNRESOLVED_WAVEFORM",
                    "Emitter '" + emitter.id + "' has unresolved waveform ref '" + *emitter.waveform_ref + "'"});
                continue;
            }
        } else {
            errors.push_back(Error{
                ErrorCategory::Planning,
                "E_PLAN_NO_WAVEFORM",
                "Emitter '" + emitter.id + "' has no waveform definition"});
            continue;
        }

        RenderInstruction instr;
        instr.emitter_id = emitter.id;
        instr.waveform = std::move(resolved_waveform);
        instr.start_sec = start;
        instr.duration_sec = emitter.duration_sec;
        instr.sample_rate = device.rf.rate_sps;
        instr.resample_ratio = std::nullopt;
        result.render_instructions.push_back(std::move(instr));

        double end_time = start + emitter.duration_sec;
        if (end_time > max_end_time) {
            max_end_time = end_time;
        }

        if (std::abs(start) < 1e-12) {
            result.warnings.push_back(Error{
                ErrorCategory::QualityWarning,
                "W_PLAN_ZERO_START",
                "Emitter '" + emitter.id + "' starts at time 0 with no preparation margin"});
        }
    }

    std::sort(result.timeline.begin(), result.timeline.end(),
              [](const TimelineEvent& a, const TimelineEvent& b) {
                  return a.time_sec < b.time_sec;
              });

    result.estimated_duration_sec = max_end_time;

    return result;
}

} // namespace archerfish::scenario
