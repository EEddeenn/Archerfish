#include "archerfish/scenario/planner.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>

#include "archerfish/dsp/waveform_type.hpp"

namespace archerfish::scenario {

namespace {

constexpr int kMaxRepeatCount = 1024;

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
    double start_sec{0.0};
    double end_sec{0.0};
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

std::optional<size_t> estimated_sample_bytes(double sample_rate, double duration_sec) {
    if (!std::isfinite(sample_rate) || !std::isfinite(duration_sec) ||
        sample_rate <= 0.0 || duration_sec < 0.0) {
        return std::nullopt;
    }

    const long double samples = std::round(static_cast<long double>(sample_rate) *
                                           static_cast<long double>(duration_sec));
    const long double bytes = samples * static_cast<long double>(sizeof(std::complex<float>));
    if (!std::isfinite(samples) || !std::isfinite(bytes) || samples < 0.0L ||
        bytes > static_cast<long double>(std::numeric_limits<size_t>::max())) {
        return std::nullopt;
    }

    return static_cast<size_t>(bytes);
}

std::optional<double> number_param(const nlohmann::json& params, const char* key) {
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
        return parsed >= 0 && parsed <= static_cast<std::int64_t>(std::numeric_limits<uint32_t>::max());
    }
    return value.get<std::uint64_t>() <= std::numeric_limits<uint32_t>::max();
}

std::optional<uint32_t> event_channel_value(const nlohmann::json& payload) {
    if (!payload.contains("channel") || !is_uint32_json(payload.at("channel"))) {
        return std::nullopt;
    }
    if (payload.at("channel").is_number_integer()) {
        return static_cast<uint32_t>(payload.at("channel").get<std::int64_t>());
    }
    return static_cast<uint32_t>(payload.at("channel").get<std::uint64_t>());
}

std::optional<double> required_positive_event_number(const ScenarioEvent& event,
                                                     const char* key,
                                                     common::ErrorList& errors) {
    if (!event.payload.contains(key)) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_INVALID_EVENT_PAYLOAD",
            "Event '" + event.type + "' missing required payload field '" + key + "'"});
        return std::nullopt;
    }
    if (!event.payload.at(key).is_number()) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_INVALID_EVENT_PAYLOAD",
            "Event '" + event.type + "' payload field '" + key + "' must be numeric"});
        return std::nullopt;
    }
    const double value = event.payload.at(key).get<double>();
    if (!std::isfinite(value) || value <= 0.0) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_INVALID_EVENT_PAYLOAD",
            "Event '" + event.type + "' payload field '" + key + "' must be finite and > 0"});
        return std::nullopt;
    }
    return value;
}

std::optional<double> required_event_number(const ScenarioEvent& event,
                                            const char* key,
                                            common::ErrorList& errors) {
    if (!event.payload.contains(key)) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_INVALID_EVENT_PAYLOAD",
            "Event '" + event.type + "' missing required payload field '" + key + "'"});
        return std::nullopt;
    }
    if (!event.payload.at(key).is_number()) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_INVALID_EVENT_PAYLOAD",
            "Event '" + event.type + "' payload field '" + key + "' must be numeric"});
        return std::nullopt;
    }
    const double value = event.payload.at(key).get<double>();
    if (!std::isfinite(value)) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_INVALID_EVENT_PAYLOAD",
            "Event '" + event.type + "' payload field '" + key + "' must be finite"});
        return std::nullopt;
    }
    return value;
}

void require_event_string(const ScenarioEvent& event, const char* key, common::ErrorList& errors) {
    if (!event.payload.contains(key)) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_INVALID_EVENT_PAYLOAD",
            "Event '" + event.type + "' missing required payload field '" + key + "'"});
        return;
    }
    if (!event.payload.at(key).is_string()) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_INVALID_EVENT_PAYLOAD",
            "Event '" + event.type + "' payload field '" + key + "' must be a string"});
    }
}

void validate_planner_event(const ScenarioEvent& event,
                            const std::unordered_set<std::string>& device_ids,
                            const std::unordered_set<std::string>& emitter_ids,
                            common::ErrorList& errors) {
    if (!std::isfinite(event.time_sec) || event.time_sec < 0.0) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_INVALID_EVENT_TIME",
            "Event time_sec must be finite and >= 0"});
    }
    if (!device_ids.count(event.target_device)) {
        errors.push_back(common::Error{
            common::ErrorCategory::Planning,
            "E_PLAN_EVENT_UNKNOWN_DEVICE",
            "Event targets unknown device '" + event.target_device + "'"});
    }

    if (event.type == "retune") {
        (void)required_positive_event_number(event, "freq_hz", errors);
        if (event.payload.contains("channel") && !is_uint32_json(event.payload.at("channel"))) {
            errors.push_back(common::Error{
                common::ErrorCategory::Planning,
                "E_PLAN_INVALID_EVENT_PAYLOAD",
                "Event 'retune' payload field 'channel' must be an unsigned 32-bit integer"});
        }
        return;
    }
    if (event.type == "gain_change") {
        (void)required_event_number(event, "gain_db", errors);
        if (event.payload.contains("channel") && !is_uint32_json(event.payload.at("channel"))) {
            errors.push_back(common::Error{
                common::ErrorCategory::Planning,
                "E_PLAN_INVALID_EVENT_PAYLOAD",
                "Event 'gain_change' payload field 'channel' must be an unsigned 32-bit integer"});
        }
        return;
    }
    if (event.type == "marker") {
        if (event.payload.contains("name") && !event.payload.at("name").is_string()) {
            errors.push_back(common::Error{
                common::ErrorCategory::Planning,
                "E_PLAN_INVALID_EVENT_PAYLOAD",
                "Event 'marker' payload field 'name' must be a string"});
        }
        return;
    }
    if (event.type == "waveform_switch") {
        require_event_string(event, "emitter_id", errors);
        require_event_string(event, "new_waveform", errors);
        if (event.payload.contains("emitter_id") && event.payload.at("emitter_id").is_string() &&
            !emitter_ids.count(event.payload.at("emitter_id").get<std::string>())) {
            errors.push_back(common::Error{
                common::ErrorCategory::Planning,
                "E_PLAN_EVENT_UNKNOWN_EMITTER",
                "Event 'waveform_switch' references unknown emitter '" +
                    event.payload.at("emitter_id").get<std::string>() + "'"});
        }
        return;
    }
    if (event.type == "impairment_change") {
        require_event_string(event, "emitter_id", errors);
        require_event_string(event, "impairment", errors);
        if (event.payload.contains("enabled") && !event.payload.at("enabled").is_boolean()) {
            errors.push_back(common::Error{
                common::ErrorCategory::Planning,
                "E_PLAN_INVALID_EVENT_PAYLOAD",
                "Event 'impairment_change' payload field 'enabled' must be a boolean"});
        }
        if (event.payload.contains("emitter_id") && event.payload.at("emitter_id").is_string() &&
            !emitter_ids.count(event.payload.at("emitter_id").get<std::string>())) {
            errors.push_back(common::Error{
                common::ErrorCategory::Planning,
                "E_PLAN_EVENT_UNKNOWN_EMITTER",
                "Event 'impairment_change' references unknown emitter '" +
                    event.payload.at("emitter_id").get<std::string>() + "'"});
        }
        return;
    }

    errors.push_back(common::Error{
        common::ErrorCategory::Planning,
        "E_PLAN_INVALID_EVENT_TYPE",
        "Unknown event type '" + event.type + "'"});
}

void add_peak_memory_estimate(size_t& peak_memory, size_t bytes) {
    if (bytes > std::numeric_limits<size_t>::max() - peak_memory) {
        peak_memory = std::numeric_limits<size_t>::max();
        return;
    }
    peak_memory += bytes;
}

std::vector<ChannelDef> build_effective_channels(const Scenario& scenario) {
    std::vector<ChannelDef> channels = scenario.channel_defs;
    if (!channels.empty()) {
        return channels;
    }

    for (const auto& dev : scenario.devices) {
        ChannelDef cd;
        cd.id = dev.id + "_ch" + std::to_string(dev.channel.value_or(0));
        cd.device = dev.id;
        cd.index = dev.channel.value_or(0);
        cd.rf = dev.rf;
        channels.push_back(std::move(cd));
    }

    return channels;
}

std::unordered_map<std::string, const ChannelDef*> build_channel_map(const std::vector<ChannelDef>& channels) {
    std::unordered_map<std::string, const ChannelDef*> channel_map;
    for (const auto& channel : channels) {
        channel_map[channel.id] = &channel;
    }
    return channel_map;
}

const ChannelDef* channel_for_emitter(
    const EmitterDef& emitter,
    const std::vector<ChannelDef>& channels,
    const std::unordered_map<std::string, const ChannelDef*>& channel_map) {
    if (emitter.channel_id.has_value()) {
        auto it = channel_map.find(*emitter.channel_id);
        if (it != channel_map.end()) {
            return it->second;
        }
        return nullptr;
    }

    auto it = std::find_if(channels.begin(), channels.end(), [&](const ChannelDef& channel) {
        return channel.device == emitter.device && channel.index == emitter.channel;
    });
    return it == channels.end() ? nullptr : &*it;
}

ChannelKey channel_key_for_emitter(
    const EmitterDef& emitter,
    const std::vector<ChannelDef>& channels,
    const std::unordered_map<std::string, const ChannelDef*>& channel_map) {
    if (const auto* channel = channel_for_emitter(emitter, channels, channel_map)) {
        return {channel->device, channel->index};
    }
    return {emitter.device, emitter.channel};
}

std::vector<EmitterWindow> emitter_windows(const EmitterDef& emitter) {
    int repeat_count = 1;
    double repeat_interval = 0.0;
    if (emitter.repeat.has_value()) {
        repeat_count = emitter.repeat->count;
        repeat_interval = emitter.repeat->interval_sec;
    }
    if (repeat_count < 1 || repeat_count > kMaxRepeatCount ||
        !std::isfinite(repeat_interval) || repeat_interval < 0.0 ||
        (repeat_count > 1 && repeat_interval <= 0.0) ||
        !std::isfinite(emitter.start_after_sec) ||
        !std::isfinite(emitter.duration_sec)) {
        return {};
    }

    std::vector<EmitterWindow> windows;
    windows.reserve(static_cast<size_t>(repeat_count));
    for (int r = 0; r < repeat_count; ++r) {
        const double start = emitter.start_after_sec + static_cast<double>(r) * repeat_interval;
        const double end = start + emitter.duration_sec;
        if (!std::isfinite(start) || !std::isfinite(end)) {
            return {};
        }
        windows.push_back({start, end});
    }
    return windows;
}

common::ErrorList reject_unschedulable_overlaps(
    const Scenario& scenario,
    const std::vector<ChannelDef>& channels,
    const std::unordered_map<std::string, const ChannelDef*>& channel_map) {
    std::unordered_map<ChannelKey, std::vector<size_t>, ChannelKeyHash> by_channel;
    for (size_t i = 0; i < scenario.emitters.size(); ++i) {
        by_channel[channel_key_for_emitter(scenario.emitters[i], channels, channel_map)].push_back(i);
    }

    common::ErrorList errors;
    for (const auto& [key, indices] : by_channel) {
        for (size_t i = 0; i < indices.size(); ++i) {
            for (size_t j = i + 1; j < indices.size(); ++j) {
                const auto& a = scenario.emitters[indices[i]];
                const auto& b = scenario.emitters[indices[j]];
                if (a.mixing == MixingMode::Additive && b.mixing == MixingMode::Additive) {
                    continue;
                }

                const auto a_windows = emitter_windows(a);
                const auto b_windows = emitter_windows(b);
                for (const auto& aw : a_windows) {
                    for (const auto& bw : b_windows) {
                        if (aw.start_sec < bw.end_sec && bw.start_sec < aw.end_sec) {
                            errors.push_back(common::Error{
                                common::ErrorCategory::Validation,
                                "V002_OVERLAPPING_EMITTERS",
                                "Emitters '" + a.id + "' and '" + b.id +
                                    "' overlap on device '" + key.device +
                                    "' channel " + std::to_string(key.channel)});
                            return errors;
                        }
                    }
                }
            }
        }
    }
    return errors;
}

void compute_resource_estimates(Plan& result, const Scenario& scenario,
                                const std::vector<ChannelDef>& channels,
                                const std::unordered_map<std::string, const ChannelDef*>& channel_map) {
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
        if (auto bytes = estimated_sample_bytes(rate, dur)) {
            add_peak_memory_estimate(peak_memory, *bytes);
        } else {
            peak_memory = std::numeric_limits<size_t>::max();
            est.warnings.emplace_back("Estimated peak memory exceeded size_t range");
        }

        for (const auto& em : scenario.emitters) {
            if (em.id == instr.emitter_id) {
                windows[channel_key_for_emitter(em, channels, channel_map)].emplace_back(instr.start_sec, instr.start_sec + dur);
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
    for (const auto& channel : result.channels) {
        if (channel.rf.rate_sps > max_rate) max_rate = channel.rf.rate_sps;
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

struct MixMemberInterval {
    std::string instruction_id;
    double start_sec{0.0};
    double end_sec{0.0};
    double amplitude{0.0};
};

void build_mix_groups(Plan& result, const Scenario& scenario,
                      const std::vector<ChannelDef>& channels,
                      const std::unordered_map<std::string, const ChannelDef*>& channel_map) {
    std::unordered_map<ChannelKey, std::vector<MixMemberInterval>, ChannelKeyHash> by_channel;
    std::unordered_map<std::string, double> instruction_amplitudes;
    for (const auto& instr : result.render_instructions) {
        instruction_amplitudes[instr.emitter_id] =
            number_param(instr.waveform.params, "amplitude").value_or(0.0);
    }

    for (const auto& em : scenario.emitters) {
        if (em.mixing == MixingMode::Additive) {
            int repeat_count = 1;
            double repeat_interval = 0.0;
            if (em.repeat.has_value()) {
                repeat_count = em.repeat->count;
                repeat_interval = em.repeat->interval_sec;
            }
            if (repeat_count < 1 || repeat_count > kMaxRepeatCount ||
                !std::isfinite(repeat_interval) || repeat_interval < 0.0 ||
                (repeat_count > 1 && repeat_interval <= 0.0)) {
                continue;
            }

            auto& intervals = by_channel[channel_key_for_emitter(em, channels, channel_map)];
            for (int r = 0; r < repeat_count; ++r) {
                const double start = em.start_after_sec + static_cast<double>(r) * repeat_interval;
                const double end = start + em.duration_sec;
                if (!std::isfinite(start) || !std::isfinite(end)) {
                    continue;
                }
                const std::string instr_id = (repeat_count > 1) ? fmt::format("{}_{}", em.id, r) : em.id;
                auto amp_it = instruction_amplitudes.find(instr_id);
                intervals.push_back({
                    instr_id,
                    start,
                    end,
                    amp_it == instruction_amplitudes.end() ? 0.0 : amp_it->second,
                });
            }
        }
    }

    for (auto& [key, intervals] : by_channel) {
        if (intervals.size() < 2) continue;

        std::sort(intervals.begin(), intervals.end(),
                  [](const auto& a, const auto& b) { return a.start_sec < b.start_sec; });

        std::vector<size_t> group;
        double group_start = intervals[0].start_sec;
        double group_end = intervals[0].end_sec;

        for (size_t i = 0; i < intervals.size(); ++i) {
            double start_i = intervals[i].start_sec;
            double end_i = intervals[i].end_sec;

            if (i == 0) {
                group.push_back(i);
                continue;
            }

            if (start_i < group_end) {
                group.push_back(i);
                group_end = std::max(group_end, end_i);
            } else {
                if (group.size() >= 2) {
                    MixGroup mg;
                    mg.device_id = key.device;
                    mg.channel = key.channel;
                    mg.start_sec = group_start;
                    mg.duration_sec = group_end - group_start;
                    double peak_sum = 0.0;
                    for (size_t interval_idx : group) {
                        const auto& member = intervals[interval_idx];
                        mg.emitter_ids.push_back(member.instruction_id);
                        peak_sum += member.amplitude;
                    }
                    mg.estimated_peak_sum = peak_sum;
                    result.mix_groups.push_back(std::move(mg));
                }
                group.clear();
                group.push_back(i);
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
            for (size_t interval_idx : group) {
                const auto& member = intervals[interval_idx];
                mg.emitter_ids.push_back(member.instruction_id);
                peak_sum += member.amplitude;
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
    std::unordered_set<std::string> device_ids;
    device_ids.reserve(scenario.devices.size());
    for (const auto& dev : scenario.devices) {
        device_ids.insert(dev.id);
    }

    std::unordered_set<std::string> emitter_ids;
    emitter_ids.reserve(scenario.emitters.size());
    for (const auto& emitter : scenario.emitters) {
        emitter_ids.insert(emitter.id);
    }

    for (const auto& event : scenario.events) {
        validate_planner_event(event, device_ids, emitter_ids, errors);
    }

    std::unordered_map<std::string, const WaveformDef*> waveform_map;
    for (const auto& wf : scenario.waveforms) {
        if (wf.id.has_value()) {
            waveform_map[*wf.id] = &wf;
        }
    }

    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }

    const auto effective_channels = build_effective_channels(scenario);
    const auto channel_map = build_channel_map(effective_channels);
    for (const auto& event : scenario.events) {
        if ((event.type == "retune" || event.type == "gain_change") &&
            event.payload.contains("channel") &&
            is_uint32_json(event.payload.at("channel"))) {
            const auto channel = *event_channel_value(event.payload);
            const bool channel_bound = std::any_of(
                effective_channels.begin(), effective_channels.end(),
                [&](const ChannelDef& cdef) {
                    return cdef.device == event.target_device && cdef.index == channel;
                });
            if (!channel_bound) {
                errors.push_back(Error{
                    ErrorCategory::Planning,
                    "E_PLAN_EVENT_UNKNOWN_CHANNEL",
                    "Event '" + event.type + "' references channel " +
                        std::to_string(channel) + " not bound to device '" +
                        event.target_device + "'"});
            }
        }
    }
    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }
    auto overlap_errors = reject_unschedulable_overlaps(scenario, effective_channels, channel_map);
    if (!overlap_errors.empty()) {
        return std::unexpected(std::move(overlap_errors));
    }

    Plan result;
    result.normalized_scenario = scenario;

    for (const auto& channel : effective_channels) {
        ChannelBinding binding;
        binding.device_id = channel.device;
        binding.channel_index = channel.index;
        binding.rf = channel.rf;
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
        if (evt.type == "marker" && evt.payload.contains("name") &&
            evt.payload["name"].is_string()) {
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
        if (repeat_count < 1) {
            errors.push_back(Error{
                ErrorCategory::Planning,
                "E_PLAN_INVALID_REPEAT",
                "Emitter '" + emitter.id + "' has repeat count < 1"});
            continue;
        }
        if (repeat_count > kMaxRepeatCount) {
            errors.push_back(Error{
                ErrorCategory::Planning,
                "E_PLAN_INVALID_REPEAT",
                "Emitter '" + emitter.id + "' has repeat count > " + std::to_string(kMaxRepeatCount)});
            continue;
        }
        if (!std::isfinite(repeat_interval) || repeat_interval < 0.0 ||
            (repeat_count > 1 && repeat_interval <= 0.0)) {
            errors.push_back(Error{
                ErrorCategory::Planning,
                "E_PLAN_INVALID_REPEAT",
                "Emitter '" + emitter.id + "' has invalid repeat interval"});
            continue;
        }
        if (!std::isfinite(emitter.start_after_sec) || emitter.start_after_sec < 0.0 ||
            !std::isfinite(emitter.duration_sec) || emitter.duration_sec <= 0.0) {
            errors.push_back(Error{
                ErrorCategory::Planning,
                "E_PLAN_INVALID_TIMING",
                "Emitter '" + emitter.id + "' has invalid start or duration"});
            continue;
        }

        for (int r = 0; r < repeat_count; ++r) {
            double start = emitter.start_after_sec + r * repeat_interval;
            double stop = start + emitter.duration_sec;
            if (!std::isfinite(start) || !std::isfinite(stop)) {
                errors.push_back(Error{
                    ErrorCategory::Planning,
                    "E_PLAN_INVALID_TIMING",
                    "Emitter '" + emitter.id + "' produced non-finite start or stop time"});
                continue;
            }

            std::string instr_id = (repeat_count > 1)
                                       ? fmt::format("{}_{}", emitter.id, r)
                                       : emitter.id;

            TimelineEvent start_event;
            start_event.type = TimelineEventType::EmitterStart;
            start_event.time_sec = start;
            start_event.target_id = instr_id;
            start_event.payload["device_id"] = emitter.device;
            start_event.payload["channel"] = emitter.channel;
            if (const auto* channel = channel_for_emitter(emitter, effective_channels, channel_map)) {
                start_event.payload["device_id"] = channel->device;
                start_event.payload["channel"] = channel->index;
                start_event.payload["channel_id"] = channel->id;
            }
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
            if (const auto* channel = channel_for_emitter(emitter, effective_channels, channel_map)) {
                instr.sample_rate = channel->rf.rate_sps;
            }
            if (!std::isfinite(instr.sample_rate) || instr.sample_rate <= 0.0) {
                errors.push_back(Error{
                    ErrorCategory::Planning,
                    "E_PLAN_INVALID_SAMPLE_RATE",
                    "Emitter '" + emitter.id + "' has invalid sample rate"});
                continue;
            }
            if (!estimated_sample_bytes(instr.sample_rate, instr.duration_sec).has_value()) {
                errors.push_back(Error{
                    ErrorCategory::Planning,
                    "E_PLAN_SAMPLE_COUNT_TOO_LARGE",
                    "Emitter '" + emitter.id + "' sample count is too large to render"});
                continue;
            }
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

    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }

    std::sort(result.timeline.begin(), result.timeline.end(),
              [](const TimelineEvent& a, const TimelineEvent& b) {
                  return a.time_sec < b.time_sec;
              });

    result.estimated_duration_sec = max_end_time;

    build_mix_groups(result, scenario, effective_channels, channel_map);
    compute_resource_estimates(result, scenario, effective_channels, channel_map);

    {
        std::unordered_map<std::string, std::string> emitter_to_channel;
        for (const auto& emitter : scenario.emitters) {
            std::string ch_id;
            if (const auto* channel = channel_for_emitter(emitter, effective_channels, channel_map)) {
                ch_id = channel->id;
            }

            int repeat_count = 1;
            if (emitter.repeat.has_value()) {
                repeat_count = emitter.repeat->count;
            }
            if (repeat_count < 1 || repeat_count > kMaxRepeatCount) {
                continue;
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
                    const auto event_channel = event_channel_value(evt.payload);
                    if (cdef.device == evt.target_id &&
                        (!event_channel.has_value() || *event_channel == cdef.index)) {
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
                if (auto bytes = estimated_sample_bytes(rate, dur)) {
                    add_peak_memory_estimate(peak_memory, *bytes);
                } else {
                    peak_memory = std::numeric_limits<size_t>::max();
                }
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
