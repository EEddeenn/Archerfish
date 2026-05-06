#include "archerfish/runtime/runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <exception>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include "archerfish/dsp/source_base.hpp"
#include "archerfish/dsp/waveform_type.hpp"
#include "archerfish/runtime/event_dispatcher.hpp"

namespace archerfish::runtime {
namespace {

enum class RuntimeScheduleKind {
    Render,
    MixGroup,
};

struct RuntimeScheduleEntry {
    double start_sec{0.0};
    double duration_sec{0.0};
    RuntimeScheduleKind kind{RuntimeScheduleKind::Render};
    size_t index{0};
};

struct RuntimeRenderWindow {
    std::string emitter_id;
    uint32_t channel{0};
    double start_sec{0.0};
    double end_sec{0.0};
};

struct RuntimeMixWindow {
    std::string label;
    uint32_t channel{0};
    double start_sec{0.0};
    double end_sec{0.0};
    std::vector<std::string> emitter_ids;
};

bool fail_runtime(StateMachine& state_machine) {
    return state_machine.transition_to(RuntimeState::Failed);
}

bool stop_tx_noexcept(hal::IHalDevice& device, uint32_t channel, const char* context) {
    try {
        device.stop_tx(channel);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("{} failed to stop TX on channel {}: {}", context, channel, e.what());
        return false;
    } catch (...) {
        spdlog::error("{} failed to stop TX on channel {}", context, channel);
        return false;
    }
}

std::optional<size_t> total_sample_count(double sample_rate, double duration_sec) {
    try {
        return dsp::SourceBase::checked_sample_count(sample_rate, duration_sec);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<size_t> mix_member_offset_samples(const RenderJob& member_job,
                                                const MixGroupJob& mix_group,
                                                size_t total_samples) {
    if (!std::isfinite(member_job.start_sec) || !std::isfinite(mix_group.start_sec)) {
        return std::nullopt;
    }

    const double relative_start_sec = member_job.start_sec - mix_group.start_sec;
    if (relative_start_sec < 0.0) {
        return std::nullopt;
    }

    auto offset = total_sample_count(member_job.sample_rate, relative_start_sec);
    if (!offset.has_value()) {
        return std::nullopt;
    }
    if (member_job.duration_sec > 0.0 && *offset >= total_samples) {
        return std::nullopt;
    }
    return offset;
}

std::optional<uint32_t> event_channel_param(const nlohmann::json& payload) {
    if (!payload.contains("channel")) {
        return std::nullopt;
    }
    const auto& value = payload.at("channel");
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        spdlog::warn("Ignoring event channel with invalid type");
        return std::nullopt;
    }
    if (value.is_number_integer()) {
        const auto parsed = value.get<std::int64_t>();
        if (parsed < 0 || parsed > static_cast<std::int64_t>(std::numeric_limits<uint32_t>::max())) {
            spdlog::warn("Ignoring event channel outside uint32 range");
            return std::nullopt;
        }
        return static_cast<uint32_t>(parsed);
    }
    const auto parsed = value.get<std::uint64_t>();
    if (parsed > std::numeric_limits<uint32_t>::max()) {
        spdlog::warn("Ignoring event channel outside uint32 range");
        return std::nullopt;
    }
    return static_cast<uint32_t>(parsed);
}

uint32_t event_channel_or_default(const nlohmann::json& payload, uint32_t fallback) {
    auto channel = event_channel_param(payload);
    return channel.has_value() ? *channel : fallback;
}

std::optional<double> event_number_param(const nlohmann::json& payload, const char* key) {
    if (!payload.contains(key)) {
        return std::nullopt;
    }
    const auto& value = payload.at(key);
    if (!value.is_number()) {
        spdlog::warn("Ignoring event payload '{}' with invalid type", key);
        return std::nullopt;
    }
    const auto parsed = value.get<double>();
    if (!std::isfinite(parsed)) {
        spdlog::warn("Ignoring event payload '{}' with non-finite value", key);
        return std::nullopt;
    }
    return parsed;
}

std::string event_string_or_default(const nlohmann::json& payload,
                                    const char* key,
                                    std::string fallback) {
    if (!payload.contains(key)) {
        return fallback;
    }
    const auto& value = payload.at(key);
    if (!value.is_string()) {
        spdlog::warn("Ignoring event payload '{}' with invalid type", key);
        return fallback;
    }
    return value.get<std::string>();
}

bool has_non_empty_string_payload(const nlohmann::json& payload, const char* key) {
    return payload.contains(key) && payload.at(key).is_string() &&
           !payload.at(key).get<std::string>().empty();
}

bool event_bool_or_default(const nlohmann::json& payload, const char* key, bool fallback) {
    if (!payload.contains(key)) {
        return fallback;
    }
    const auto& value = payload.at(key);
    if (!value.is_boolean()) {
        spdlog::warn("Ignoring event payload '{}' with invalid type", key);
        return fallback;
    }
    return value.get<bool>();
}

const scenario::ChannelPlan* find_channel_plan_by_index(const scenario::Plan& plan, uint32_t channel) {
    auto it = std::find_if(plan.channel_plans.begin(), plan.channel_plans.end(),
                           [channel](const scenario::ChannelPlan& cp) {
                               return cp.channel_index == channel;
                           });
    return it != plan.channel_plans.end() ? &*it : nullptr;
}

double channel_sample_rate_or_default(const scenario::Plan& plan, uint32_t channel, double fallback) {
    auto it = std::find_if(plan.channels.begin(), plan.channels.end(),
                           [channel](const scenario::ChannelBinding& binding) {
                               return binding.channel_index == channel;
                           });
    return it != plan.channels.end() ? it->rf.rate_sps : fallback;
}

bool is_channel_rf_event(scenario::TimelineEventType type) {
    return type == scenario::TimelineEventType::FreqChange ||
           type == scenario::TimelineEventType::GainChange;
}

std::vector<uint32_t> active_channel_indices(const scenario::Plan& plan) {
    std::vector<uint32_t> channels;
    std::unordered_set<uint32_t> seen;
    for (const auto& ch : plan.channels) {
        if (seen.insert(ch.channel_index).second) {
            channels.push_back(ch.channel_index);
        }
    }
    return channels;
}

bool intervals_overlap(double a_start, double a_end, double b_start, double b_end) {
    return a_start < b_end && b_start < a_end;
}

bool mix_group_contains(const scenario::MixGroup& group, const std::string& emitter_id) {
    return std::find(group.emitter_ids.begin(), group.emitter_ids.end(), emitter_id) !=
           group.emitter_ids.end();
}

bool same_mix_group_covers_overlap(const scenario::Plan& plan,
                                   const RuntimeRenderWindow& a,
                                   const RuntimeRenderWindow& b) {
    for (const auto& group : plan.mix_groups) {
        if (group.channel != a.channel) {
            continue;
        }
        if (!mix_group_contains(group, a.emitter_id) ||
            !mix_group_contains(group, b.emitter_id)) {
            continue;
        }

        const double group_end = group.start_sec + group.duration_sec;
        const double overlap_start = std::max(a.start_sec, b.start_sec);
        const double overlap_end = std::min(a.end_sec, b.end_sec);
        if (group.start_sec <= overlap_start && group_end >= overlap_end) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> validate_runtime_waveform_params(const scenario::Plan& plan) {
    auto validate = [](const scenario::RenderInstruction& instr,
                       const std::string& field) -> std::optional<std::string> {
        if (instr.waveform.params.is_null()) {
            return std::nullopt;
        }
        if (!instr.waveform.params.is_object()) {
            return field + " waveform params must be an object";
        }
        if (instr.waveform.params.contains("type")) {
            return field + " waveform params contain reserved key 'type'";
        }
        return std::nullopt;
    };

    for (const auto& instr : plan.render_instructions) {
        if (auto error = validate(instr, "render instruction '" + instr.emitter_id + "'")) {
            return error;
        }
    }
    for (const auto& cp : plan.channel_plans) {
        for (const auto& instr : cp.render_instructions) {
            if (auto error = validate(instr,
                                      "channel plan render instruction '" + instr.emitter_id + "'")) {
                return error;
            }
        }
    }
    return std::nullopt;
}

nlohmann::json render_job_waveform_config(const scenario::WaveformDef& waveform) {
    nlohmann::json config = waveform.params.is_object() ? waveform.params : nlohmann::json::object();
    config["type"] = dsp::to_string(waveform.type);
    return config;
}

std::optional<std::string> validate_runtime_overlap_schedule(const scenario::Plan& plan) {
    const auto channels = active_channel_indices(plan);
    if (channels.empty()) {
        return std::nullopt;
    }

    std::vector<RuntimeRenderWindow> windows;
    if (channels.size() == 1) {
        windows.reserve(plan.render_instructions.size());
        for (const auto& instr : plan.render_instructions) {
            windows.push_back({instr.emitter_id, channels.front(), instr.start_sec,
                               instr.start_sec + instr.duration_sec});
        }
    } else {
        for (size_t ci = 0; ci < channels.size(); ++ci) {
            const uint32_t channel = channels[ci];
            if (const auto* cp = find_channel_plan_by_index(plan, channel)) {
                for (const auto& instr : cp->render_instructions) {
                    windows.push_back({instr.emitter_id, channel, instr.start_sec,
                                       instr.start_sec + instr.duration_sec});
                }
            } else {
                for (size_t i = ci; i < plan.render_instructions.size(); i += channels.size()) {
                    const auto& instr = plan.render_instructions[i];
                    windows.push_back({instr.emitter_id, channel, instr.start_sec,
                                       instr.start_sec + instr.duration_sec});
                }
            }
        }
    }

    for (size_t i = 0; i < windows.size(); ++i) {
        const auto& a = windows[i];
        if (!std::isfinite(a.start_sec) || !std::isfinite(a.end_sec)) {
            return "render instruction '" + a.emitter_id +
                   "' has non-finite start or duration";
        }
        for (size_t j = i + 1; j < windows.size(); ++j) {
            const auto& b = windows[j];
            if (a.channel != b.channel) {
                continue;
            }
            if (!intervals_overlap(a.start_sec, a.end_sec, b.start_sec, b.end_sec)) {
                continue;
            }
            if (same_mix_group_covers_overlap(plan, a, b)) {
                continue;
            }
            return "render instructions '" + a.emitter_id + "' and '" + b.emitter_id +
                   "' overlap on channel " + std::to_string(a.channel) +
                   " without a covering mix group";
        }
    }

    std::vector<RuntimeMixWindow> mix_windows;
    mix_windows.reserve(plan.mix_groups.size());
    for (const auto& group : plan.mix_groups) {
        mix_windows.push_back({group.device_id + ":" + std::to_string(group.channel),
                               group.channel,
                               group.start_sec,
                               group.start_sec + group.duration_sec,
                               group.emitter_ids});
    }
    for (size_t i = 0; i < mix_windows.size(); ++i) {
        const auto& a = mix_windows[i];
        if (!std::isfinite(a.start_sec) || !std::isfinite(a.end_sec)) {
            return "mix group '" + a.label + "' has non-finite start or duration";
        }
        for (size_t j = i + 1; j < mix_windows.size(); ++j) {
            const auto& b = mix_windows[j];
            if (a.channel == b.channel &&
                intervals_overlap(a.start_sec, a.end_sec, b.start_sec, b.end_sec)) {
                return "mix groups on channel " + std::to_string(a.channel) +
                       " overlap and cannot be transmitted concurrently";
            }
        }
    }

    for (const auto& group : mix_windows) {
        for (const auto& window : windows) {
            if (group.channel != window.channel) {
                continue;
            }
            if (std::find(group.emitter_ids.begin(), group.emitter_ids.end(),
                          window.emitter_id) != group.emitter_ids.end()) {
                continue;
            }
            if (intervals_overlap(group.start_sec, group.end_sec,
                                  window.start_sec, window.end_sec)) {
                return "mix group on channel " + std::to_string(group.channel) +
                       " overlaps render instruction '" + window.emitter_id +
                       "' outside the group";
            }
        }
    }

    return std::nullopt;
}

std::string event_schedule_key(const scenario::TimelineEvent& event, std::optional<uint32_t> channel) {
    std::ostringstream key;
    key << static_cast<int>(event.type) << '|'
        << event.time_sec << '|'
        << event.target_id << '|';
    if (channel.has_value()) {
        key << *channel;
    }
    key << '|' << event.payload.dump();
    return key.str();
}

std::vector<RuntimeScheduleEntry> build_runtime_schedule(const std::vector<RenderJob>& render_jobs,
                                                         const std::vector<size_t>& render_indices,
                                                         const std::vector<MixGroupJob>& mix_group_jobs) {
    std::vector<RuntimeScheduleEntry> schedule;
    schedule.reserve(render_indices.size() + mix_group_jobs.size());
    for (const auto index : render_indices) {
        const auto& job = render_jobs[index];
        schedule.push_back({job.start_sec, job.duration_sec, RuntimeScheduleKind::Render, index});
    }
    for (size_t i = 0; i < mix_group_jobs.size(); ++i) {
        const auto& job = mix_group_jobs[i];
        schedule.push_back({job.start_sec, job.duration_sec, RuntimeScheduleKind::MixGroup, i});
    }
    std::stable_sort(schedule.begin(), schedule.end(),
                     [](const RuntimeScheduleEntry& a, const RuntimeScheduleEntry& b) {
                         return a.start_sec < b.start_sec;
                     });
    return schedule;
}

} // namespace

Runtime::Runtime(std::shared_ptr<hal::IHalDevice> device, RuntimeConfig config)
    : device_(std::move(device)), config_(config) {}

bool Runtime::prepare(const scenario::Plan& plan) {
    if (!device_) {
        spdlog::error("Runtime prepare failed: no HAL device configured");
        (void)fail_runtime(state_machine_);
        return false;
    }
    if (config_.queue_capacity == 0) {
        spdlog::error("Runtime prepare failed: queue capacity must be greater than zero");
        (void)fail_runtime(state_machine_);
        return false;
    }
    std::unordered_set<std::string> device_ids;
    std::unordered_set<uint32_t> runtime_channels;
    for (const auto& ch : plan.channels) {
        device_ids.insert(ch.device_id);
        if (!runtime_channels.insert(ch.channel_index).second) {
            spdlog::error("Runtime prepare failed: duplicate runtime channel index {}",
                          ch.channel_index);
            (void)fail_runtime(state_machine_);
            return false;
        }
    }
    if (device_ids.size() > 1) {
        spdlog::error("Runtime prepare failed: a single Runtime instance cannot execute a plan "
                      "with multiple device ids");
        (void)fail_runtime(state_machine_);
        return false;
    }

    if (!state_machine_.transition_to(RuntimeState::Validated)) return false;
    if (!state_machine_.transition_to(RuntimeState::Planned)) return false;

    try {
        current_plan_ = plan;

        queue_ = std::make_unique<SampleQueue>(config_.queue_capacity);

        for (const auto& ch : plan.channels) {
            auto rf_errors = ch.rf.validate();
            if (!rf_errors.empty()) {
                spdlog::error("Runtime prepare failed: invalid RF settings for channel {}: {}",
                              ch.channel_index, rf_errors.front().message);
                (void)fail_runtime(state_machine_);
                return false;
            }
            device_->set_center_freq(ch.channel_index, ch.rf.freq_hz);
            device_->set_sample_rate(ch.channel_index, ch.rf.rate_sps);
            device_->set_gain(ch.channel_index, ch.rf.gain_db);
            if (ch.rf.bandwidth_hz.has_value()) {
                device_->set_bandwidth(ch.channel_index, *ch.rf.bandwidth_hz);
            }
            if (ch.rf.antenna.has_value()) {
                device_->set_antenna(ch.channel_index, *ch.rf.antenna);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Runtime prepare failed: {}", e.what());
        (void)fail_runtime(state_machine_);
        return false;
    }

    if (!state_machine_.transition_to(RuntimeState::Prepared)) {
        return false;
    }
    return true;
}

bool Runtime::arm() {
    if (config_.block_size == 0) {
        spdlog::error("Runtime arm failed: block size must be greater than zero");
        (void)fail_runtime(state_machine_);
        return false;
    }
    if (current_plan_.render_instructions.empty()) {
        spdlog::error("Runtime arm failed: no render instructions in plan");
        (void)fail_runtime(state_machine_);
        return false;
    }

    if (!state_machine_.transition_to(RuntimeState::Armed)) {
        return false;
    }

    try {
        render_jobs_.clear();
        std::unordered_set<std::string> render_ids;
        std::unordered_set<std::string> channel_bindings;
        std::unordered_set<uint32_t> channel_indices;
        std::unordered_set<std::string> device_ids;
        for (const auto& ch : current_plan_.channels) {
            channel_bindings.insert(ch.device_id + ":" + std::to_string(ch.channel_index));
            channel_indices.insert(ch.channel_index);
            device_ids.insert(ch.device_id);
        }
        for (const auto& instr : current_plan_.render_instructions) {
            if (!render_ids.insert(instr.emitter_id).second) {
                spdlog::error("Runtime arm failed: duplicate render instruction emitter id '{}'",
                              instr.emitter_id);
                (void)fail_runtime(state_machine_);
                return false;
            }
            RenderJob job;
            job.emitter_id = instr.emitter_id;
            job.waveform_config = render_job_waveform_config(instr.waveform);
            job.sample_rate = instr.sample_rate;
            job.duration_sec = instr.duration_sec;
            job.start_sec = instr.start_sec;
            job.block_size = config_.block_size;
            job.impairments = instr.impairments;
            render_jobs_.push_back(std::move(job));
        }
        std::unordered_set<uint32_t> channel_plan_indices;
        std::unordered_set<std::string> channel_plan_render_ids;
        for (const auto& cp : current_plan_.channel_plans) {
            if (!channel_indices.count(cp.channel_index)) {
                spdlog::error("Runtime arm failed: channel plan references unknown channel {}",
                              cp.channel_index);
                (void)fail_runtime(state_machine_);
                return false;
            }
            if (!channel_plan_indices.insert(cp.channel_index).second) {
                spdlog::error("Runtime arm failed: duplicate channel plan for channel {}",
                              cp.channel_index);
                (void)fail_runtime(state_machine_);
                return false;
            }
            std::unordered_set<std::string> channel_render_ids;
            for (const auto& instr : cp.render_instructions) {
                if (!channel_render_ids.insert(instr.emitter_id).second) {
                    spdlog::error("Runtime arm failed: duplicate channel plan render instruction '{}'",
                                  instr.emitter_id);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
                if (!channel_plan_render_ids.insert(instr.emitter_id).second) {
                    spdlog::error("Runtime arm failed: render instruction '{}' appears in multiple channel plans",
                                  instr.emitter_id);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
                if (!render_ids.count(instr.emitter_id)) {
                    spdlog::error("Runtime arm failed: channel plan references unknown render instruction '{}'",
                                  instr.emitter_id);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
            }
            for (const auto& evt : cp.events) {
                if (evt.type == scenario::TimelineEventType::EmitterStart ||
                    evt.type == scenario::TimelineEventType::EmitterStop) {
                    if (!channel_render_ids.count(evt.target_id)) {
                        spdlog::error("Runtime arm failed: channel plan event references unknown emitter '{}'",
                                      evt.target_id);
                        (void)fail_runtime(state_machine_);
                        return false;
                    }
                    continue;
                }

                const auto binding = evt.target_id + ":" + std::to_string(cp.channel_index);
                if (!channel_bindings.count(binding)) {
                    spdlog::error("Runtime arm failed: channel plan event references device/channel not bound to channel plan '{}'",
                                  binding);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
                if (auto event_channel = event_channel_param(evt.payload);
                    event_channel.has_value() && *event_channel != cp.channel_index) {
                    spdlog::error("Runtime arm failed: channel plan event channel {} does not match channel plan {}",
                                  *event_channel, cp.channel_index);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
                if (evt.payload.contains("channel") && !event_channel_param(evt.payload).has_value()) {
                    spdlog::error("Runtime arm failed: channel plan event channel has invalid payload");
                    (void)fail_runtime(state_machine_);
                    return false;
                }

                if (evt.type == scenario::TimelineEventType::FreqChange) {
                    auto freq = event_number_param(evt.payload, "freq_hz");
                    if (!freq.has_value() || *freq <= 0.0) {
                        spdlog::error("Runtime arm failed: channel plan FreqChange event requires positive freq_hz");
                        (void)fail_runtime(state_machine_);
                        return false;
                    }
                } else if (evt.type == scenario::TimelineEventType::GainChange) {
                    if (!event_number_param(evt.payload, "gain_db").has_value()) {
                        spdlog::error("Runtime arm failed: channel plan GainChange event requires numeric gain_db");
                        (void)fail_runtime(state_machine_);
                        return false;
                    }
                } else if (evt.type == scenario::TimelineEventType::Marker) {
                    if (evt.payload.contains("name") &&
                        (!evt.payload["name"].is_string() ||
                         evt.payload["name"].get<std::string>().empty())) {
                        spdlog::error("Runtime arm failed: channel plan Marker event name must be a non-empty string");
                        (void)fail_runtime(state_machine_);
                        return false;
                    }
                } else if (evt.type == scenario::TimelineEventType::WaveformSwitch) {
                    if (!has_non_empty_string_payload(evt.payload, "emitter_id") ||
                        !has_non_empty_string_payload(evt.payload, "new_waveform")) {
                        spdlog::error("Runtime arm failed: channel plan WaveformSwitch event has invalid payload");
                        (void)fail_runtime(state_machine_);
                        return false;
                    }
                    const auto emitter_id = evt.payload["emitter_id"].get<std::string>();
                    if (!channel_render_ids.count(emitter_id)) {
                        spdlog::error("Runtime arm failed: channel plan WaveformSwitch references unknown emitter '{}'",
                                      emitter_id);
                        (void)fail_runtime(state_machine_);
                        return false;
                    }
                } else if (evt.type == scenario::TimelineEventType::ImpairmentChange) {
                    if (!has_non_empty_string_payload(evt.payload, "emitter_id") ||
                        !has_non_empty_string_payload(evt.payload, "impairment") ||
                        (evt.payload.contains("enabled") && !evt.payload["enabled"].is_boolean())) {
                        spdlog::error("Runtime arm failed: channel plan ImpairmentChange event has invalid payload");
                        (void)fail_runtime(state_machine_);
                        return false;
                    }
                    const auto emitter_id = evt.payload["emitter_id"].get<std::string>();
                    if (!channel_render_ids.count(emitter_id)) {
                        spdlog::error("Runtime arm failed: channel plan ImpairmentChange references unknown emitter '{}'",
                                      emitter_id);
                        (void)fail_runtime(state_machine_);
                        return false;
                    }
                }
            }
        }
        if (!current_plan_.channel_plans.empty()) {
            for (const auto& emitter_id : render_ids) {
                if (!channel_plan_render_ids.count(emitter_id)) {
                    spdlog::error("Runtime arm failed: render instruction '{}' is missing from channel plans",
                                  emitter_id);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
            }
        }
        for (const auto& evt : current_plan_.timeline) {
            if (evt.type == scenario::TimelineEventType::EmitterStart ||
                evt.type == scenario::TimelineEventType::EmitterStop) {
                if (!render_ids.count(evt.target_id)) {
                    spdlog::error("Runtime arm failed: timeline references unknown emitter '{}'",
                                  evt.target_id);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
                continue;
            }
            if (!device_ids.count(evt.target_id)) {
                spdlog::error("Runtime arm failed: timeline references unknown device '{}'",
                              evt.target_id);
                (void)fail_runtime(state_machine_);
                return false;
            }
            if (auto event_channel = event_channel_param(evt.payload);
                event_channel.has_value() &&
                !channel_bindings.count(evt.target_id + ":" + std::to_string(*event_channel))) {
                spdlog::error("Runtime arm failed: timeline references unknown channel '{}:{}'",
                              evt.target_id, *event_channel);
                (void)fail_runtime(state_machine_);
                return false;
            }
            if (evt.payload.contains("channel") && !event_channel_param(evt.payload).has_value()) {
                spdlog::error("Runtime arm failed: timeline event channel has invalid payload");
                (void)fail_runtime(state_machine_);
                return false;
            }
            if (evt.type == scenario::TimelineEventType::FreqChange) {
                auto freq = event_number_param(evt.payload, "freq_hz");
                if (!freq.has_value() || *freq <= 0.0) {
                    spdlog::error("Runtime arm failed: FreqChange timeline event requires positive freq_hz");
                    (void)fail_runtime(state_machine_);
                    return false;
                }
            } else if (evt.type == scenario::TimelineEventType::GainChange) {
                if (!event_number_param(evt.payload, "gain_db").has_value()) {
                    spdlog::error("Runtime arm failed: GainChange timeline event requires numeric gain_db");
                    (void)fail_runtime(state_machine_);
                    return false;
                }
            } else if (evt.type == scenario::TimelineEventType::Marker) {
                if (evt.payload.contains("name") &&
                    (!evt.payload["name"].is_string() ||
                     evt.payload["name"].get<std::string>().empty())) {
                    spdlog::error("Runtime arm failed: Marker timeline event name must be a non-empty string");
                    (void)fail_runtime(state_machine_);
                    return false;
                }
            } else if (evt.type == scenario::TimelineEventType::WaveformSwitch) {
                if (!has_non_empty_string_payload(evt.payload, "emitter_id") ||
                    !has_non_empty_string_payload(evt.payload, "new_waveform")) {
                    spdlog::error("Runtime arm failed: WaveformSwitch timeline event requires non-empty string emitter_id and new_waveform");
                    (void)fail_runtime(state_machine_);
                    return false;
                }
                const auto emitter_id = evt.payload["emitter_id"].get<std::string>();
                if (!render_ids.count(emitter_id)) {
                    spdlog::error("Runtime arm failed: WaveformSwitch references unknown emitter '{}'",
                                  emitter_id);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
            } else if (evt.type == scenario::TimelineEventType::ImpairmentChange) {
                if (!has_non_empty_string_payload(evt.payload, "emitter_id") ||
                    !has_non_empty_string_payload(evt.payload, "impairment") ||
                    (evt.payload.contains("enabled") && !evt.payload["enabled"].is_boolean())) {
                    spdlog::error("Runtime arm failed: ImpairmentChange timeline event has invalid payload");
                    (void)fail_runtime(state_machine_);
                    return false;
                }
                const auto emitter_id = evt.payload["emitter_id"].get<std::string>();
                if (!render_ids.count(emitter_id)) {
                    spdlog::error("Runtime arm failed: ImpairmentChange references unknown emitter '{}'",
                                  emitter_id);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
            }
        }

    if (auto overlap_error = validate_runtime_overlap_schedule(current_plan_)) {
        spdlog::error("Runtime arm failed: {}", *overlap_error);
        (void)fail_runtime(state_machine_);
        return false;
    }
    if (auto waveform_params_error = validate_runtime_waveform_params(current_plan_)) {
        spdlog::error("Runtime arm failed: {}", *waveform_params_error);
        (void)fail_runtime(state_machine_);
        return false;
    }

        mix_group_jobs_.clear();
        for (const auto& mg : current_plan_.mix_groups) {
            const auto binding = mg.device_id + ":" + std::to_string(mg.channel);
            if (!channel_bindings.count(binding)) {
                spdlog::error("Runtime arm failed: mix group references unknown channel '{}'",
                              binding);
                (void)fail_runtime(state_machine_);
                return false;
            }
            MixGroupJob mgj;
            mgj.device_id = mg.device_id;
            mgj.channel = mg.channel;
            mgj.start_sec = mg.start_sec;
            mgj.duration_sec = mg.duration_sec;
            mgj.estimated_peak_sum = mg.estimated_peak_sum;
            std::unordered_set<std::string> mix_member_ids;
            for (const auto& eid : mg.emitter_ids) {
                if (!mix_member_ids.insert(eid).second) {
                    spdlog::error("Runtime arm failed: mix group contains duplicate emitter '{}'", eid);
                    (void)fail_runtime(state_machine_);
                    return false;
                }
                bool found = false;
                for (const auto& instr : current_plan_.render_instructions) {
                    if (instr.emitter_id == eid) {
                        RenderJob job;
                        job.emitter_id = instr.emitter_id;
                        job.waveform_config = render_job_waveform_config(instr.waveform);
                        job.sample_rate = instr.sample_rate;
                        job.duration_sec = instr.duration_sec;
                        job.start_sec = instr.start_sec;
                        job.block_size = config_.block_size;
                        job.impairments = instr.impairments;
                        mgj.member_jobs.push_back(std::move(job));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (mg.estimated_peak_sum > 0.0) {
                        spdlog::error("Runtime arm failed: mix group references unknown emitter '{}'", eid);
                        (void)fail_runtime(state_machine_);
                        return false;
                    }
                    spdlog::warn("Skipping stale zero-peak mix group emitter reference '{}'", eid);
                }
            }
            mix_group_jobs_.push_back(std::move(mgj));
        }
    } catch (const std::exception& e) {
        spdlog::error("Runtime arm failed: {}", e.what());
        (void)fail_runtime(state_machine_);
        return false;
    }

    return true;
}

bool Runtime::run() {
    if (!state_machine_.transition_to(RuntimeState::Running)) {
        return false;
    }

    auto active_channels = get_active_channels(current_plan_);

    bool success = false;
    try {
        if (config_.run_mode == scenario::RunMode::Replay || current_plan_.run_mode == scenario::RunMode::Replay) {
            if (active_channels.size() > 1) {
                spdlog::error("Runtime replay failed: replay mode supports only one active channel");
                success = false;
            } else {
                success = run_replay(active_channels.empty() ? config_.channel : active_channels.front());
            }
        } else if (active_channels.size() > 1) {
            success = run_multi_channel(active_channels);
        } else {
            success = run_single_channel(active_channels.empty() ? config_.channel : active_channels.front());
        }
    } catch (const std::exception& e) {
        spdlog::error("Runtime run failed: {}", e.what());
        success = false;
    } catch (...) {
        spdlog::error("Runtime run failed with an unknown error");
        success = false;
    }

    if (!success && state_machine_.current() != RuntimeState::Failed) {
        (void)fail_runtime(state_machine_);
    }
    return success;
}

std::vector<uint32_t> Runtime::get_active_channels(const scenario::Plan& plan) {
    std::vector<uint32_t> channels;
    std::unordered_set<uint32_t> seen;
    for (const auto& ch : plan.channels) {
        if (seen.insert(ch.channel_index).second) {
            channels.push_back(ch.channel_index);
        }
    }
    return channels;
}

void Runtime::schedule_timeline_event(EventDispatcher& dispatcher,
                                      const scenario::TimelineEvent& evt,
                                      uint32_t default_channel) {
    if (evt.type == scenario::TimelineEventType::FreqChange) {
        auto freq = event_number_param(evt.payload, "freq_hz");
        if (!freq.has_value() || *freq <= 0.0) {
            spdlog::warn("Skipping freq change event with invalid freq_hz");
            return;
        }
        uint32_t ch = event_channel_or_default(evt.payload, default_channel);
        dispatcher.schedule(evt.time_sec, [this, ch, freq = *freq]() {
            device_->set_center_freq(ch, freq);
        });
    } else if (evt.type == scenario::TimelineEventType::GainChange) {
        auto gain = event_number_param(evt.payload, "gain_db");
        if (!gain.has_value()) {
            spdlog::warn("Skipping gain change event with invalid gain_db");
            return;
        }
        uint32_t ch = event_channel_or_default(evt.payload, default_channel);
        dispatcher.schedule(evt.time_sec, [this, ch, gain = *gain]() {
            device_->set_gain(ch, gain);
        });
    } else if (evt.type == scenario::TimelineEventType::Marker) {
        std::string name = event_string_or_default(evt.payload, "name", evt.target_id);
        double planned_time = evt.time_sec;
        dispatcher.schedule(evt.time_sec, [this, name, planned_time]() {
            auto now = std::chrono::system_clock::now();
            double wall_sec = std::chrono::duration<double>(now.time_since_epoch()).count();
            spdlog::info("[MARKER] {} @ {}", name, wall_sec);
            MarkerDispatch md;
            md.name = name;
            md.planned_time_sec = planned_time;
            md.wall_clock_sec = wall_sec;
            marker_dispatches_.push_back(std::move(md));
        });
    } else if (evt.type == scenario::TimelineEventType::WaveformSwitch) {
        std::string emitter_id = event_string_or_default(evt.payload, "emitter_id", "");
        std::string new_waveform = event_string_or_default(evt.payload, "new_waveform", "");
        double planned_time = evt.time_sec;
        dispatcher.schedule(evt.time_sec, [this, emitter_id, new_waveform, planned_time]() {
            auto now = std::chrono::system_clock::now();
            double wall_sec = std::chrono::duration<double>(now.time_since_epoch()).count();
            spdlog::info("[WAVEFORM_SWITCH] emitter={} new_waveform={} @ {}", emitter_id, new_waveform, wall_sec);
            WaveformSwitchDispatch wsd;
            wsd.emitter_id = emitter_id;
            wsd.new_waveform = new_waveform;
            wsd.planned_time_sec = planned_time;
            wsd.wall_clock_sec = wall_sec;
            waveform_switch_dispatches_.push_back(std::move(wsd));
        });
    } else if (evt.type == scenario::TimelineEventType::ImpairmentChange) {
        std::string emitter_id = event_string_or_default(evt.payload, "emitter_id", "");
        std::string impairment = event_string_or_default(evt.payload, "impairment", "");
        bool enabled = event_bool_or_default(evt.payload, "enabled", true);
        double planned_time = evt.time_sec;
        dispatcher.schedule(evt.time_sec, [this, emitter_id, impairment, enabled, planned_time]() {
            auto now = std::chrono::system_clock::now();
            double wall_sec = std::chrono::duration<double>(now.time_since_epoch()).count();
            spdlog::info("[IMPAIRMENT_CHANGE] emitter={} impairment={} enabled={} @ {}",
                         emitter_id, impairment, enabled, wall_sec);
            ImpairmentChangeDispatch icd;
            icd.emitter_id = emitter_id;
            icd.impairment = impairment;
            icd.enabled = enabled;
            icd.planned_time_sec = planned_time;
            icd.wall_clock_sec = wall_sec;
            impairment_change_dispatches_.push_back(std::move(icd));
        });
    }
}

bool Runtime::run_single_channel(uint32_t channel) {
    auto run_start = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point tx_start_time;
    bool tx_started = false;
    bool render_failed = false;
    bool stop_failed = false;
    bool event_failed = false;
    auto stoken = stop_source_.get_token();

    EventDispatcher dispatcher;
    for (const auto& evt : current_plan_.timeline) {
        schedule_timeline_event(dispatcher, evt, channel);
    }
    if (dispatcher.total_events() > 0) {
        dispatcher.start();
    }

    std::unordered_set<std::string> mix_group_emitter_ids;
    for (const auto& mg : current_plan_.mix_groups) {
        for (const auto& eid : mg.emitter_ids) {
            mix_group_emitter_ids.insert(eid);
        }
    }

    std::vector<size_t> solo_job_indices;
    for (size_t i = 0; i < render_jobs_.size(); ++i) {
        if (mix_group_emitter_ids.find(render_jobs_[i].emitter_id) == mix_group_emitter_ids.end()) {
            solo_job_indices.push_back(i);
        }
    }
    const auto schedule = build_runtime_schedule(render_jobs_, solo_job_indices, mix_group_jobs_);

    auto ensure_tx_started = [&]() {
        if (!tx_started) {
            device_->start_tx(channel);
            tx_start_time = std::chrono::steady_clock::now();
            tx_started = true;
        }
    };

    auto transmit_render_job = [&](size_t job_idx) {
        const auto& job = render_jobs_[job_idx];
        ensure_tx_started();

        queue_ = std::make_unique<SampleQueue>(config_.queue_capacity);
        RenderWorker render_worker(*queue_, job, stop_source_);

        render_worker.start();

        {
            std::unique_lock lock(queue_->cv_mutex_);
            queue_->cv_not_empty_.wait(lock, [&] {
                return queue_->size() >= config_.queue_capacity / 2 || render_worker.is_complete();
            });
        }

        {
            std::lock_guard lock(active_tx_worker_mutex_);
            active_tx_worker_ = std::make_unique<TxWorker>(*queue_, *device_, channel, stop_source_);
            active_tx_worker_->start();
        }

        render_worker.join();
        if (!stoken.stop_requested() && render_worker.has_failed()) {
            spdlog::error("Runtime render failed for emitter '{}'", job.emitter_id);
            render_failed = true;
        }

        std::unique_ptr<TxWorker> local_worker;
        {
            std::lock_guard lock(active_tx_worker_mutex_);
            if (active_tx_worker_) {
                if (render_worker.has_failed()) {
                    active_tx_worker_->request_stop();
                } else if (render_worker.samples_rendered() == 0) {
                    SampleBlock sentinel;
                    sentinel.end_of_burst = true;
                    sentinel.start_of_burst = false;
                    queue_->push_wait(std::move(sentinel), stoken);
                }
                local_worker = std::move(active_tx_worker_);
            }
        }
        if (local_worker) {
            local_worker->join();
            metrics_.total_samples_sent += local_worker->metrics().samples_sent.load();
            metrics_.total_blocks_sent += local_worker->metrics().blocks_sent.load();
            metrics_.underruns += local_worker->metrics().underruns.load();
            if (local_worker->metrics().failed.load(std::memory_order_acquire)) {
                render_failed = true;
            }
            auto expected_samples = total_sample_count(job.sample_rate, job.duration_sec);
            if (!stoken.stop_requested() && expected_samples.has_value() && *expected_samples > 0 &&
                local_worker->metrics().samples_sent.load() == 0) {
                spdlog::error("Runtime transmitted zero samples for emitter '{}'", job.emitter_id);
                render_failed = true;
            }
        }
        if (render_failed) {
            return;
        }
    };

    auto transmit_mix_group = [&](size_t mix_group_idx) {
        const auto& mgj = mix_group_jobs_[mix_group_idx];
        if (mgj.member_jobs.empty()) return;
        ensure_tx_started();

        auto total_samples_opt = total_sample_count(mgj.member_jobs.front().sample_rate, mgj.duration_sec);
        if (!total_samples_opt.has_value()) {
            spdlog::error("Runtime failed mix group with invalid sample count");
            render_failed = true;
            return;
        }
        size_t total_samples = *total_samples_opt;
        std::vector<std::complex<float>> mixed_buffer(total_samples, {0.0f, 0.0f});

        for (const auto& member_job : mgj.member_jobs) {
            auto member_offset = mix_member_offset_samples(member_job, mgj, total_samples);
            if (!member_offset.has_value()) {
                spdlog::error("Runtime failed mix group '{}': emitter '{}' has invalid relative timing",
                              mgj.device_id, member_job.emitter_id);
                render_failed = true;
                break;
            }
            auto member_buffer = RenderWorker::pre_render(member_job);
            if (member_buffer.empty() && member_job.duration_sec > 0.0) {
                spdlog::error("Runtime failed to pre-render mix group emitter '{}'", member_job.emitter_id);
                render_failed = true;
                break;
            }
            for (size_t s = 0; s < member_buffer.size(); ++s) {
                size_t dst = *member_offset + s;
                if (dst < total_samples) {
                    mixed_buffer[dst] += member_buffer[s];
                }
            }
        }
        if (render_failed) {
            return;
        }

        queue_ = std::make_unique<SampleQueue>(config_.queue_capacity);

        {
            std::lock_guard lock(active_tx_worker_mutex_);
            active_tx_worker_ = std::make_unique<TxWorker>(*queue_, *device_, channel, stop_source_);
            active_tx_worker_->start();
        }

        size_t offset = 0;
        while (offset < total_samples) {
            if (stoken.stop_requested()) break;

            size_t block_sz = std::min(total_samples - offset, config_.block_size);
            SampleBlock block;
            block.samples.assign(mixed_buffer.begin() + static_cast<ptrdiff_t>(offset),
                                 mixed_buffer.begin() + static_cast<ptrdiff_t>(offset + block_sz));
            block.start_of_burst = (offset == 0);
            block.end_of_burst = (offset + block_sz >= total_samples);

            if (!queue_->push_wait(std::move(block), stoken)) break;
            offset += block_sz;
        }

        SampleBlock sentinel;
        sentinel.end_of_burst = true;
        sentinel.start_of_burst = false;
        queue_->push_wait(std::move(sentinel), stoken);

        std::unique_ptr<TxWorker> local_worker;
        {
            std::lock_guard lock(active_tx_worker_mutex_);
            if (active_tx_worker_) {
                local_worker = std::move(active_tx_worker_);
            }
        }
        if (local_worker) {
            local_worker->join();
            metrics_.total_samples_sent += local_worker->metrics().samples_sent.load();
            metrics_.total_blocks_sent += local_worker->metrics().blocks_sent.load();
            metrics_.underruns += local_worker->metrics().underruns.load();
            if (local_worker->metrics().failed.load(std::memory_order_acquire)) {
                render_failed = true;
            }
        }
    };

    double previous_end_sec = 0.0;
    bool has_previous = false;
    for (const auto& entry : schedule) {
        if (stoken.stop_requested() || render_failed) break;

        const double delay = has_previous ? entry.start_sec - previous_end_sec : entry.start_sec;
        if (delay > 0.0) {
            std::unique_lock lock(abort_mutex_);
            abort_cv_.wait_for(lock, std::chrono::duration<double>(delay),
                               [this] { return stop_source_.stop_requested(); });
            if (stop_source_.stop_requested()) break;
        }

        if (entry.kind == RuntimeScheduleKind::Render) {
            transmit_render_job(entry.index);
        } else {
            transmit_mix_group(entry.index);
        }
        previous_end_sec = std::max(previous_end_sec, entry.start_sec + entry.duration_sec);
        has_previous = true;
    }

    if (dispatcher.total_events() > 0) {
        dispatcher.wait_complete();
        event_failed = dispatcher.has_failed();
        auto dispatched_markers = dispatcher.marker_dispatches();
        for (const auto& md : dispatched_markers) {
            marker_dispatches_.push_back(md);
        }
    }

    if (tx_started) {
        double rate = 1e6;
        for (const auto& binding : current_plan_.channels) {
            if (binding.channel_index == channel) {
                rate = binding.rf.rate_sps;
                break;
            }
        }
        double expected_air_time = static_cast<double>(metrics_.total_samples_sent) / rate;
        auto tx_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - tx_start_time).count();
        double drain_remaining = expected_air_time - tx_elapsed;
        if (drain_remaining > 0.0) {
            std::unique_lock lock(abort_mutex_);
            abort_cv_.wait_for(lock, std::chrono::duration<double>(drain_remaining),
                               [this] { return stop_source_.stop_requested(); });
        }
        stop_failed = !stop_tx_noexcept(*device_, channel, "Runtime");
    }

    auto run_stop = std::chrono::steady_clock::now();

    auto start_sec = std::chrono::duration<double>(run_start.time_since_epoch()).count();
    auto stop_sec = std::chrono::duration<double>(run_stop.time_since_epoch()).count();

    metrics_.actual_start_sec = start_sec;
    metrics_.actual_stop_sec = stop_sec;
    metrics_.actual_duration_sec = stop_sec - start_sec;

    if (render_failed || stop_failed || event_failed) {
        return false;
    }

    (void)state_machine_.transition_to(RuntimeState::Completed);
    return true;
}

bool Runtime::run_multi_channel(const std::vector<uint32_t>& channels) {
    auto run_start = std::chrono::steady_clock::now();
    auto stoken = stop_source_.get_token();
    EventDispatcher dispatcher;
    std::unordered_set<std::string> scheduled_events;
    bool event_failed = false;

    auto schedule_once = [&](const scenario::TimelineEvent& event, uint32_t default_channel) {
        std::optional<uint32_t> effective_channel;
        if (is_channel_rf_event(event.type)) {
            effective_channel = event_channel_or_default(event.payload, default_channel);
        }
        const auto key = event_schedule_key(event, effective_channel);
        if (scheduled_events.insert(key).second) {
            schedule_timeline_event(dispatcher, event, default_channel);
        }
    };

    const uint32_t first_channel = channels.empty() ? config_.channel : channels.front();
    for (const auto& event : current_plan_.timeline) {
        if (is_channel_rf_event(event.type) &&
            !event_channel_param(event.payload).has_value() &&
            !current_plan_.channel_plans.empty()) {
            continue;
        }
        schedule_once(event, first_channel);
    }

    for (uint32_t channel : channels) {
        const auto* cp = find_channel_plan_by_index(current_plan_, channel);
        if (cp == nullptr) {
            continue;
        }
        for (const auto& event : cp->events) {
            schedule_once(event, cp->channel_index);
        }
    }

    if (dispatcher.total_events() > 0) {
        dispatcher.start();
    }

    // Create per-channel executors
    std::vector<ChannelExecutor> executors;
    for (size_t ci = 0; ci < channels.size(); ++ci) {
        ChannelExecutor exec;
        exec.channel_index = channels[ci];
        exec.queue = std::make_unique<SampleQueue>(config_.queue_capacity);
        exec.metrics.channel_index = channels[ci];

        if (const auto* cp = find_channel_plan_by_index(current_plan_, channels[ci])) {
            for (const auto& instr : cp->render_instructions) {
                RenderJob job;
                job.emitter_id = instr.emitter_id;
                job.waveform_config = render_job_waveform_config(instr.waveform);
                job.sample_rate = instr.sample_rate;
                job.duration_sec = instr.duration_sec;
                job.start_sec = instr.start_sec;
                job.block_size = config_.block_size;
                job.impairments = instr.impairments;
                exec.render_jobs.push_back(std::move(job));
            }
        } else {
            for (size_t i = ci; i < render_jobs_.size(); i += channels.size()) {
                exec.render_jobs.push_back(render_jobs_[i]);
            }
        }

        for (const auto& mgj : mix_group_jobs_) {
            if (mgj.channel == channels[ci]) {
                exec.mix_group_jobs.push_back(mgj);
            }
        }

        executors.push_back(std::move(exec));
    }

    std::vector<uint32_t> started_channels;
    started_channels.reserve(channels.size());
    try {
        for (uint32_t ch : channels) {
            device_->start_tx(ch);
            started_channels.push_back(ch);
        }
    } catch (...) {
        for (auto it = started_channels.rbegin(); it != started_channels.rend(); ++it) {
            (void)stop_tx_noexcept(*device_, *it, "Runtime rollback");
        }
        throw;
    }
    auto tx_start_time = std::chrono::steady_clock::now();

    std::vector<std::thread> channel_threads;
    for (auto& exec : executors) {
        auto* exec_ptr = &exec;
        channel_threads.emplace_back([this, exec_ptr, stoken]() {
            execute_channel_jobs(*exec_ptr, stoken);
        });
    }

    for (auto& t : channel_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (dispatcher.total_events() > 0) {
        dispatcher.wait_complete();
        event_failed = dispatcher.has_failed();
    }

    for (auto& exec : executors) {
        metrics_.total_samples_sent += exec.metrics.samples_sent;
        metrics_.total_blocks_sent += exec.metrics.blocks_sent;
        metrics_.underruns += exec.metrics.underruns;
        metrics_.per_channel.push_back(exec.metrics);
    }

    bool stop_failed = false;
    if (!channels.empty()) {
        // Each channel drains at its own rate; wait for the slowest
        double max_drain_sec = 0.0;
        for (const auto& exec : executors) {
            double rate = channel_sample_rate_or_default(current_plan_, exec.channel_index, 0.0);
            if (rate > 0.0) {
                double ch_air_time = static_cast<double>(exec.metrics.samples_sent) / rate;
                max_drain_sec = std::max(max_drain_sec, ch_air_time);
            }
        }
        auto tx_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - tx_start_time).count();
        double drain_remaining = max_drain_sec - tx_elapsed;
        if (drain_remaining > 0.0) {
            std::unique_lock lock(abort_mutex_);
            abort_cv_.wait_for(lock, std::chrono::duration<double>(drain_remaining),
                               [this] { return stop_source_.stop_requested(); });
        }
        for (uint32_t ch : channels) {
            stop_failed = !stop_tx_noexcept(*device_, ch, "Runtime") || stop_failed;
        }
    }

    auto run_stop = std::chrono::steady_clock::now();
    metrics_.actual_start_sec = std::chrono::duration<double>(run_start.time_since_epoch()).count();
    metrics_.actual_stop_sec = std::chrono::duration<double>(run_stop.time_since_epoch()).count();
    metrics_.actual_duration_sec = std::chrono::duration<double>(run_stop - run_start).count();

    const bool failed = std::any_of(executors.begin(), executors.end(),
                                    [](const ChannelExecutor& exec) { return exec.failed; });
    if (failed || stop_failed || event_failed) {
        return false;
    }

    (void)state_machine_.transition_to(RuntimeState::Completed);
    return true;
}

void Runtime::execute_channel_jobs(ChannelExecutor& exec, std::stop_token stoken) {
    auto ch_start = std::chrono::steady_clock::now();
    std::unordered_set<std::string> mix_group_emitter_ids;
    for (const auto& group : exec.mix_group_jobs) {
        for (const auto& member_job : group.member_jobs) {
            mix_group_emitter_ids.insert(member_job.emitter_id);
        }
    }

    std::vector<size_t> render_indices;
    render_indices.reserve(exec.render_jobs.size());
    for (size_t i = 0; i < exec.render_jobs.size(); ++i) {
        if (!mix_group_emitter_ids.count(exec.render_jobs[i].emitter_id)) {
            render_indices.push_back(i);
        }
    }
    const auto schedule = build_runtime_schedule(exec.render_jobs, render_indices, exec.mix_group_jobs);

    auto transmit_render_job = [&](size_t job_idx) {
        const auto& job = exec.render_jobs[job_idx];
        auto ch_queue = std::make_unique<SampleQueue>(config_.queue_capacity);
        RenderWorker render_worker(*ch_queue, job, stop_source_);
        render_worker.start();

        {
            std::unique_lock lock(ch_queue->cv_mutex_);
            ch_queue->cv_not_empty_.wait(lock, [&] {
                return ch_queue->size() >= config_.queue_capacity / 2 || render_worker.is_complete();
            });
        }

        TxWorker tx_worker(*ch_queue, *device_, exec.channel_index, stop_source_);
        tx_worker.start();

        render_worker.join();

        if (!stoken.stop_requested() && render_worker.has_failed()) {
            spdlog::error("Runtime render failed for emitter '{}' on channel {}",
                          job.emitter_id, exec.channel_index);
            exec.failed = true;
            tx_worker.request_stop();
        } else if (render_worker.samples_rendered() == 0) {
            SampleBlock sentinel;
            sentinel.end_of_burst = true;
            sentinel.start_of_burst = false;
            ch_queue->push_wait(std::move(sentinel), stoken);
        }

        tx_worker.join();
        const size_t sent = tx_worker.metrics().samples_sent.load();
        const size_t blocks_sent = tx_worker.metrics().blocks_sent.load();
        exec.metrics.samples_sent += sent;
        exec.metrics.blocks_sent += blocks_sent;
        exec.metrics.underruns += tx_worker.metrics().underruns.load();
        if (tx_worker.metrics().failed.load(std::memory_order_acquire)) {
            exec.failed = true;
        }

        auto expected_samples = total_sample_count(job.sample_rate, job.duration_sec);
        if (!stoken.stop_requested() && expected_samples.has_value() && *expected_samples > 0 && sent == 0) {
            spdlog::error("Runtime transmitted zero samples for emitter '{}' on channel {}",
                          job.emitter_id, exec.channel_index);
            exec.failed = true;
        }

        exec.queue = std::move(ch_queue);
        if (exec.failed) {
            return;
        }
    };

    auto transmit_mix_group = [&](size_t mix_group_idx) {
        const auto& mgj = exec.mix_group_jobs[mix_group_idx];
        if (mgj.member_jobs.empty()) return;

        auto total_samples_opt = total_sample_count(mgj.member_jobs.front().sample_rate, mgj.duration_sec);
        if (!total_samples_opt.has_value()) {
            spdlog::error("Runtime failed mix group with invalid sample count on channel {}",
                          exec.channel_index);
            exec.failed = true;
            return;
        }
        size_t total_samples = *total_samples_opt;
        std::vector<std::complex<float>> mixed_buffer(total_samples, {0.0f, 0.0f});

        for (const auto& member_job : mgj.member_jobs) {
            auto member_offset = mix_member_offset_samples(member_job, mgj, total_samples);
            if (!member_offset.has_value()) {
                spdlog::error("Runtime failed mix group '{}' on channel {}: emitter '{}' has invalid relative timing",
                              mgj.device_id, exec.channel_index, member_job.emitter_id);
                exec.failed = true;
                break;
            }
            auto member_buffer = RenderWorker::pre_render(member_job);
            if (member_buffer.empty() && member_job.duration_sec > 0.0) {
                spdlog::error("Runtime failed to pre-render mix group emitter '{}' on channel {}",
                              member_job.emitter_id, exec.channel_index);
                exec.failed = true;
                break;
            }
            for (size_t s = 0; s < member_buffer.size(); ++s) {
                size_t dst = *member_offset + s;
                if (dst < total_samples) {
                    mixed_buffer[dst] += member_buffer[s];
                }
            }
        }
        if (exec.failed) {
            return;
        }

        auto mg_queue = std::make_unique<SampleQueue>(config_.queue_capacity);

        TxWorker tx_worker(*mg_queue, *device_, exec.channel_index, stop_source_);
        tx_worker.start();

        size_t offset = 0;
        while (offset < total_samples) {
            if (stoken.stop_requested()) break;

            size_t block_sz = std::min(total_samples - offset, config_.block_size);
            SampleBlock block;
            block.samples.assign(mixed_buffer.begin() + static_cast<ptrdiff_t>(offset),
                                 mixed_buffer.begin() + static_cast<ptrdiff_t>(offset + block_sz));
            block.start_of_burst = (offset == 0);
            block.end_of_burst = (offset + block_sz >= total_samples);

            if (!mg_queue->push_wait(std::move(block), stoken)) break;
            offset += block_sz;
        }

        SampleBlock sentinel;
        sentinel.end_of_burst = true;
        sentinel.start_of_burst = false;
        mg_queue->push_wait(std::move(sentinel), stoken);

        tx_worker.join();

        exec.metrics.samples_sent += tx_worker.metrics().samples_sent.load();
        exec.metrics.blocks_sent += tx_worker.metrics().blocks_sent.load();
        exec.metrics.underruns += tx_worker.metrics().underruns.load();
        if (tx_worker.metrics().failed.load(std::memory_order_acquire)) {
            exec.failed = true;
        }
    };

    double previous_end_sec = 0.0;
    bool has_previous = false;
    for (const auto& entry : schedule) {
        if (stoken.stop_requested() || exec.failed) break;

        const double delay = has_previous ? entry.start_sec - previous_end_sec : entry.start_sec;
        if (delay > 0.0) {
            std::unique_lock lock(abort_mutex_);
            abort_cv_.wait_for(lock, std::chrono::duration<double>(delay),
                               [this] { return stop_source_.stop_requested(); });
            if (stop_source_.stop_requested()) break;
        }

        if (entry.kind == RuntimeScheduleKind::Render) {
            transmit_render_job(entry.index);
        } else {
            transmit_mix_group(entry.index);
        }
        previous_end_sec = std::max(previous_end_sec, entry.start_sec + entry.duration_sec);
        has_previous = true;
    }

    auto ch_stop = std::chrono::steady_clock::now();
    exec.metrics.active_duration_sec = std::chrono::duration<double>(ch_stop - ch_start).count();
}


bool Runtime::run_replay(uint32_t channel) {
    auto run_start = std::chrono::steady_clock::now();
    auto stoken = stop_source_.get_token();
    bool event_failed = false;

    EventDispatcher dispatcher;
    for (const auto& evt : current_plan_.timeline) {
        schedule_timeline_event(dispatcher, evt, channel);
    }
    if (dispatcher.total_events() > 0) {
        dispatcher.start();
    }

    if (render_jobs_.empty()) {
        if (dispatcher.total_events() > 0) {
            dispatcher.wait_complete();
            event_failed = dispatcher.has_failed();
        }
        if (event_failed) {
            return false;
        }
        (void)state_machine_.transition_to(RuntimeState::Completed);
        return true;
    }

    std::vector<std::complex<float>> replay_buffer;
    for (const auto& job : render_jobs_) {
        auto expected_samples = total_sample_count(job.sample_rate, job.duration_sec);
        if (!expected_samples.has_value()) {
            spdlog::error("Runtime replay failed: emitter '{}' has invalid sample count",
                          job.emitter_id);
            return false;
        }
        auto partial = RenderWorker::pre_render(job);
        if (partial.empty() && *expected_samples > 0) {
            spdlog::error("Runtime replay failed to pre-render emitter '{}'", job.emitter_id);
            return false;
        }
        replay_buffer.insert(replay_buffer.end(), partial.begin(), partial.end());
    }

    if (replay_buffer.empty()) {
        if (dispatcher.total_events() > 0) {
            dispatcher.wait_complete();
            event_failed = dispatcher.has_failed();
        }
        auto run_stop = std::chrono::steady_clock::now();
        metrics_.actual_start_sec = std::chrono::duration<double>(run_start.time_since_epoch()).count();
        metrics_.actual_stop_sec = std::chrono::duration<double>(run_stop.time_since_epoch()).count();
        metrics_.actual_duration_sec = std::chrono::duration<double>(run_stop - run_start).count();

        if (event_failed) {
            return false;
        }

        (void)state_machine_.transition_to(RuntimeState::Completed);
        return true;
    }

    device_->start_tx(channel);
    auto tx_start_time = std::chrono::steady_clock::now();
    bool stop_failed = false;
    bool tx_failed = false;

    queue_ = std::make_unique<SampleQueue>(config_.queue_capacity);

    {
        std::lock_guard lock(active_tx_worker_mutex_);
        active_tx_worker_ = std::make_unique<TxWorker>(*queue_, *device_, channel, stop_source_);
        active_tx_worker_->start();
    }

    size_t offset = 0;
    while (offset < replay_buffer.size()) {
        if (stoken.stop_requested()) break;

        size_t block_sz = std::min(replay_buffer.size() - offset, config_.block_size);
        SampleBlock block;
        block.samples.assign(replay_buffer.begin() + static_cast<ptrdiff_t>(offset),
                             replay_buffer.begin() + static_cast<ptrdiff_t>(offset + block_sz));
        block.start_of_burst = (offset == 0);
        block.end_of_burst = (offset + block_sz >= replay_buffer.size());

        if (!queue_->push_wait(std::move(block), stoken)) break;
        offset += block_sz;
    }

    if (stoken.stop_requested()) {
        SampleBlock sentinel;
        sentinel.end_of_burst = true;
        sentinel.start_of_burst = false;
        queue_->push_wait(std::move(sentinel), stoken);
    }

    {
        std::lock_guard lock(active_tx_worker_mutex_);
        if (active_tx_worker_) {
            active_tx_worker_->join();
            metrics_.total_samples_sent += active_tx_worker_->metrics().samples_sent.load();
            metrics_.total_blocks_sent += active_tx_worker_->metrics().blocks_sent.load();
            metrics_.underruns += active_tx_worker_->metrics().underruns.load();
            tx_failed = active_tx_worker_->metrics().failed.load(std::memory_order_acquire);
            active_tx_worker_.reset();
        }
    }

    double rate = 1e6;
    for (const auto& binding : current_plan_.channels) {
        if (binding.channel_index == channel) {
            rate = binding.rf.rate_sps;
            break;
        }
    }
    double expected_air_time = static_cast<double>(metrics_.total_samples_sent) / rate;
    auto tx_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - tx_start_time).count();
    double drain_remaining = expected_air_time - tx_elapsed;
    if (drain_remaining > 0.0) {
        std::unique_lock lock(abort_mutex_);
        abort_cv_.wait_for(lock, std::chrono::duration<double>(drain_remaining),
                           [this] { return stop_source_.stop_requested(); });
    }
    stop_failed = !stop_tx_noexcept(*device_, channel, "Runtime replay");

    if (dispatcher.total_events() > 0) {
        dispatcher.wait_complete();
        event_failed = dispatcher.has_failed();
    }

    auto run_stop = std::chrono::steady_clock::now();
    metrics_.actual_start_sec = std::chrono::duration<double>(run_start.time_since_epoch()).count();
    metrics_.actual_stop_sec = std::chrono::duration<double>(run_stop.time_since_epoch()).count();
    metrics_.actual_duration_sec = std::chrono::duration<double>(run_stop - run_start).count();

    if (tx_failed || stop_failed || event_failed) {
        return false;
    }

    (void)state_machine_.transition_to(RuntimeState::Completed);
    return true;
}
void Runtime::abort() {
    stop_source_.request_stop();
    {
        std::lock_guard lock(abort_mutex_);
        abort_cv_.notify_all();
    }
    {
        std::lock_guard lock(active_tx_worker_mutex_);
        if (active_tx_worker_) active_tx_worker_->request_stop();
    }
    (void)state_machine_.transition_to(RuntimeState::Aborted);

    if (device_) {
        auto active_channels = get_active_channels(current_plan_);
        for (uint32_t ch : active_channels) {
            (void)stop_tx_noexcept(*device_, ch, "Runtime abort");
        }
    }
    {
        std::lock_guard lock(active_tx_worker_mutex_);
        if (active_tx_worker_) {
            active_tx_worker_->join();
            active_tx_worker_.reset();
        }
    }
}

RuntimeState Runtime::state() const {
    return state_machine_.current();
}

Runtime::RunMetrics Runtime::get_metrics() const {
    return metrics_;
}

const std::vector<MarkerDispatch>& Runtime::marker_dispatches() const {
    return marker_dispatches_;
}

const std::vector<WaveformSwitchDispatch>& Runtime::waveform_switch_dispatches() const {
    return waveform_switch_dispatches_;
}

const std::vector<ImpairmentChangeDispatch>& Runtime::impairment_change_dispatches() const {
    return impairment_change_dispatches_;
}

} // namespace archerfish::runtime
