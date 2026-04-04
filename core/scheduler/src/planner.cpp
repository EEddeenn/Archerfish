#include "archerfish/scenario/planner.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>

#include "archerfish/dsp/waveform_type.hpp"

namespace archerfish::scenario {

namespace {

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

double complexity_factor(dsp::WaveformType type) {
    switch (type) {
        case dsp::WaveformType::Chirp: return 1.5;
        case dsp::WaveformType::Noise: return 1.0;
        case dsp::WaveformType::AM:
        case dsp::WaveformType::FM:
        case dsp::WaveformType::PM:
        case dsp::WaveformType::ASK:
        case dsp::WaveformType::FSK: return 3.0;
        case dsp::WaveformType::MultiTone: return 2.0;
        default: return 1.0;
    }
}

void compute_resource_estimates(Plan& result, const Scenario& scenario,
                                const std::unordered_map<std::string, const DeviceDef*>& /*device_map*/) {
    auto& est = result.resource_estimate;
    double total_cpu = 0.0;
    size_t peak_memory = 0;

    std::unordered_map<ChannelKey, std::vector<std::pair<double, double>>, ChannelKeyHash> windows;

    double min_gap = std::numeric_limits<double>::max();

    for (const auto& instr : result.render_instructions) {
        double rate = instr.sample_rate;
        double dur = instr.duration_sec;
        double cf = complexity_factor(instr.waveform.type);

        total_cpu += rate * dur * cf;
        peak_memory += static_cast<size_t>(rate * dur * sizeof(std::complex<float>));

        std::string dev_id;
        for (const auto& em : scenario.emitters) {
            if (em.id == instr.emitter_id) {
                dev_id = em.device;
                windows[{em.device, em.channel}].emplace_back(instr.start_sec, instr.start_sec + dur);
                break;
            }
        }
    }

    for (auto& [key, wins] : windows) {
        std::sort(wins.begin(), wins.end());
        for (size_t i = 1; i < wins.size(); ++i) {
            double gap = wins[i].first - wins[i - 1].second;
            if (gap >= 0 && gap < min_gap) {
                min_gap = gap;
            }
        }
    }

    auto dev_it = scenario.devices.begin();
    (void)dev_it;
    double max_rate = 0.0;
    for (const auto& dev : scenario.devices) {
        if (dev.rf.rate_sps > max_rate) max_rate = dev.rf.rate_sps;
    }

    if (max_rate > 0.0) {
        est.estimated_cpu_load = total_cpu / (max_rate * result.estimated_duration_sec);
    } else {
        est.estimated_cpu_load = 0.0;
    }
    est.peak_memory_bytes = peak_memory;
    est.min_inter_emitter_gap_sec = (min_gap == std::numeric_limits<double>::max()) ? 0.0 : min_gap;

    est.timing_feasible = true;
    if (result.render_instructions.size() > 1 && est.min_inter_emitter_gap_sec < 10e-6) {
        est.timing_feasible = false;
        est.warnings.emplace_back("Inter-emitter gap < 10us: timing may not be feasible");
    }
    if (est.estimated_cpu_load > 1.0) {
        est.timing_feasible = false;
        est.warnings.emplace_back("Estimated CPU load > 1.0: schedule may not be feasible");
    }
    if (result.render_instructions.size() > 1 && est.min_inter_emitter_gap_sec < 100e-6 && est.min_inter_emitter_gap_sec >= 10e-6) {
        est.warnings.emplace_back("Inter-emitter gap < 100us: tight timing pressure");
    }
}

void build_mix_groups(Plan& result, const Scenario& scenario) {
    std::unordered_map<ChannelKey, std::vector<size_t>, ChannelKeyHash> by_channel;

    for (size_t i = 0; i < scenario.emitters.size(); ++i) {
        const auto& em = scenario.emitters[i];
        if (em.mixing == MixingMode::Additive) {
            by_channel[{em.device, em.channel}].push_back(i);
        }
    }

    for (const auto& [key, indices] : by_channel) {
        if (indices.size() < 2) continue;

        std::vector<std::pair<size_t, std::pair<double, double>>> intervals;
        for (size_t idx : indices) {
            const auto& em = scenario.emitters[idx];
            intervals.emplace_back(idx, std::make_pair(em.start_after_sec, em.start_after_sec + em.duration_sec));
        }
        std::sort(intervals.begin(), intervals.end(),
                  [](const auto& a, const auto& b) { return a.second.first < b.second.first; });

        std::vector<size_t> group;
        double group_start = intervals[0].second.first;
        double group_end = intervals[0].second.second;

        for (size_t i = 0; i < intervals.size(); ++i) {
            double start_i = intervals[i].second.first;
            double end_i = intervals[i].second.second;

            if (i == 0) {
                group.push_back(intervals[i].first);
                continue;
            }

            if (start_i < group_end) {
                group.push_back(intervals[i].first);
                group_end = std::max(group_end, end_i);
            } else {
                if (group.size() >= 2) {
                    MixGroup mg;
                    mg.device_id = key.device;
                    mg.channel = key.channel;
                    mg.start_sec = group_start;
                    mg.duration_sec = group_end - group_start;
                    double peak_sum = 0.0;
                    for (size_t em_idx : group) {
                        const auto& em = scenario.emitters[em_idx];
                        mg.emitter_ids.push_back(em.id);
                        if (em.waveform.has_value() && em.waveform->params.contains("amplitude")) {
                            peak_sum += em.waveform->params["amplitude"].get<double>();
                        }
                    }
                    mg.estimated_peak_sum = peak_sum;
                    result.mix_groups.push_back(std::move(mg));
                }
                group.clear();
                group.push_back(intervals[i].first);
                group_start = start_i;
                group_end = end_i;
            }
        }

        if (group.size() >= 2) {
            MixGroup mg;
            mg.device_id = key.device;
            mg.channel = key.channel;
            mg.start_sec = group_start;
            mg.duration_sec = group_end - group_start;
            double peak_sum = 0.0;
            for (size_t em_idx : group) {
                const auto& em = scenario.emitters[em_idx];
                mg.emitter_ids.push_back(em.id);
                if (em.waveform.has_value() && em.waveform->params.contains("amplitude")) {
                    peak_sum += em.waveform->params["amplitude"].get<double>();
                }
            }
            mg.estimated_peak_sum = peak_sum;
            result.mix_groups.push_back(std::move(mg));
        }
    }
}

} // namespace

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

    for (const auto& evt : scenario.events) {
        TimelineEvent te;
        if (evt.type == "retune") {
            te.type = TimelineEventType::FreqChange;
        } else if (evt.type == "gain_change") {
            te.type = TimelineEventType::GainChange;
        } else if (evt.type == "marker") {
            te.type = TimelineEventType::Marker;
        } else if (evt.type == "waveform_switch") {
            te.type = TimelineEventType::WaveformSwitch;
        } else if (evt.type == "impairment_change") {
            te.type = TimelineEventType::ImpairmentChange;
        } else {
            continue;
        }
        te.time_sec = evt.time_sec;
        te.target_id = evt.target_device;
        te.payload = evt.payload;
        if (evt.type == "marker" && evt.payload.contains("name")) {
            te.payload["name"] = evt.payload["name"].get<std::string>();
        }
        result.timeline.push_back(std::move(te));
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

        int repeat_count = 1;
        double repeat_interval = 0.0;
        if (emitter.repeat.has_value()) {
            repeat_count = emitter.repeat->count;
            repeat_interval = emitter.repeat->interval_sec;
        }

        for (int r = 0; r < repeat_count; ++r) {
            double start = emitter.start_after_sec + r * repeat_interval;
            double stop = start + emitter.duration_sec;

            std::string instr_id = (repeat_count > 1)
                                       ? fmt::format("{}_{}", emitter.id, r)
                                       : emitter.id;

            TimelineEvent start_event;
            start_event.type = TimelineEventType::EmitterStart;
            start_event.time_sec = start;
            start_event.target_id = instr_id;
            start_event.payload["device_id"] = emitter.device;
            start_event.payload["channel"] = emitter.channel;
            result.timeline.push_back(std::move(start_event));

            TimelineEvent stop_event;
            stop_event.type = TimelineEventType::EmitterStop;
            stop_event.time_sec = stop;
            stop_event.target_id = instr_id;
            result.timeline.push_back(std::move(stop_event));

            RenderInstruction instr;
            instr.emitter_id = instr_id;
            instr.waveform = resolved_waveform;
            instr.start_sec = start;
            instr.duration_sec = emitter.duration_sec;
            instr.sample_rate = device.rf.rate_sps;
            instr.resample_ratio = std::nullopt;
            instr.impairments = emitter.impairments;
            result.render_instructions.push_back(std::move(instr));

            double end_time = stop;
            if (end_time > max_end_time) {
                max_end_time = end_time;
            }

            if (std::abs(start) < 1e-12) {
                result.warnings.push_back(Error{
                    ErrorCategory::QualityWarning,
                    "W_PLAN_ZERO_START",
                    "Emitter '" + instr_id + "' starts at time 0 with no preparation margin"});
            }
        }
    }

    std::sort(result.timeline.begin(), result.timeline.end(),
              [](const TimelineEvent& a, const TimelineEvent& b) {
                  return a.time_sec < b.time_sec;
              });

    result.estimated_duration_sec = max_end_time;

    build_mix_groups(result, scenario);
    compute_resource_estimates(result, scenario, device_map);

    {
        std::vector<ChannelDef> effective_channels = scenario.channel_defs;
        if (effective_channels.empty()) {
            for (const auto& dev : scenario.devices) {
                ChannelDef cd;
                cd.id = dev.id + "_ch" + std::to_string(dev.channel.value_or(0));
                cd.device = dev.id;
                cd.index = dev.channel.value_or(0);
                cd.rf = dev.rf;
                effective_channels.push_back(std::move(cd));
            }
        }

        std::unordered_map<std::string, std::string> emitter_to_channel;
        for (const auto& emitter : scenario.emitters) {
            std::string ch_id;
            if (emitter.channel_id.has_value()) {
                ch_id = *emitter.channel_id;
            } else {
                for (const auto& cd : effective_channels) {
                    if (cd.device == emitter.device && cd.index == emitter.channel) {
                        ch_id = cd.id;
                        break;
                    }
                }
            }

            int repeat_count = 1;
            if (emitter.repeat.has_value()) {
                repeat_count = emitter.repeat->count;
            }
            for (int r = 0; r < repeat_count; ++r) {
                std::string instr_id = (repeat_count > 1)
                                           ? fmt::format("{}_{}", emitter.id, r)
                                           : emitter.id;
                emitter_to_channel[instr_id] = ch_id;
            }
        }

        for (const auto& cdef : effective_channels) {
            ChannelPlan cp;
            cp.channel_id = cdef.id;
            cp.channel_index = cdef.index;
            cp.rf = cdef.rf;

            for (const auto& instr : result.render_instructions) {
                auto it = emitter_to_channel.find(instr.emitter_id);
                if (it != emitter_to_channel.end() && it->second == cdef.id) {
                    cp.render_instructions.push_back(instr);
                }
            }

            for (const auto& evt : result.timeline) {
                if (evt.type == TimelineEventType::EmitterStart || evt.type == TimelineEventType::EmitterStop) {
                    auto it = emitter_to_channel.find(evt.target_id);
                    if (it != emitter_to_channel.end() && it->second == cdef.id) {
                        cp.events.push_back(evt);
                    }
                } else {
                    if (cdef.device == evt.target_id) {
                        cp.events.push_back(evt);
                    }
                }
            }

            double total_cpu = 0.0;
            size_t peak_memory = 0;
            double min_gap = std::numeric_limits<double>::max();

            for (const auto& instr : cp.render_instructions) {
                double rate = instr.sample_rate;
                double dur = instr.duration_sec;
                double cf = complexity_factor(instr.waveform.type);
                total_cpu += rate * dur * cf;
                peak_memory += static_cast<size_t>(rate * dur * sizeof(std::complex<float>));
            }

            if (cp.render_instructions.size() > 1) {
                std::vector<std::pair<double, double>> windows;
                for (const auto& instr : cp.render_instructions) {
                    windows.emplace_back(instr.start_sec, instr.start_sec + instr.duration_sec);
                }
                std::sort(windows.begin(), windows.end());
                for (size_t i = 1; i < windows.size(); ++i) {
                    double gap = windows[i].first - windows[i - 1].second;
                    if (gap >= 0 && gap < min_gap) {
                        min_gap = gap;
                    }
                }
            }

            double channel_duration = 0.0;
            for (const auto& instr : cp.render_instructions) {
                double end = instr.start_sec + instr.duration_sec;
                if (end > channel_duration) channel_duration = end;
            }

            auto& est = cp.resource_estimate;
            if (cdef.rf.rate_sps > 0.0 && channel_duration > 0.0) {
                est.estimated_cpu_load = total_cpu / (cdef.rf.rate_sps * channel_duration);
            }
            est.peak_memory_bytes = peak_memory;
            est.min_inter_emitter_gap_sec = (min_gap == std::numeric_limits<double>::max()) ? 0.0 : min_gap;
            est.timing_feasible = true;

            if (cp.render_instructions.size() > 1 && est.min_inter_emitter_gap_sec < 10e-6) {
                est.timing_feasible = false;
                est.warnings.emplace_back("Inter-emitter gap < 10us: timing may not be feasible");
            }
            if (est.estimated_cpu_load > 1.0) {
                est.timing_feasible = false;
                est.warnings.emplace_back("Estimated CPU load > 1.0: schedule may not be feasible");
            }

            result.channel_plans.push_back(std::move(cp));
        }
    }

    result.run_mode = scenario.run.mode;

    return result;
}

} // namespace archerfish::scenario
