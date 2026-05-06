#include "archerfish/scenario/plan_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "archerfish/dsp/waveform_type.hpp"
#include "archerfish/scenario/validator.hpp"

namespace archerfish::scenario {

namespace {

std::string timeline_event_type_to_string(TimelineEventType type) {
    switch (type) {
        case TimelineEventType::EmitterStart: return "EmitterStart";
        case TimelineEventType::EmitterStop: return "EmitterStop";
        case TimelineEventType::GainChange: return "GainChange";
        case TimelineEventType::FreqChange: return "FreqChange";
        case TimelineEventType::Marker: return "Marker";
        case TimelineEventType::WaveformSwitch: return "WaveformSwitch";
        case TimelineEventType::ImpairmentChange: return "ImpairmentChange";
    }
    return "Unknown";
}

std::expected<TimelineEventType, common::ErrorList> string_to_timeline_event_type(const std::string& s) {
    if (s == "EmitterStart") return TimelineEventType::EmitterStart;
    if (s == "EmitterStop") return TimelineEventType::EmitterStop;
    if (s == "GainChange") return TimelineEventType::GainChange;
    if (s == "FreqChange") return TimelineEventType::FreqChange;
    if (s == "Marker") return TimelineEventType::Marker;
    if (s == "WaveformSwitch") return TimelineEventType::WaveformSwitch;
    if (s == "ImpairmentChange") return TimelineEventType::ImpairmentChange;
    return std::unexpected(common::ErrorList{
        {common::ErrorCategory::Planning, "E_PLAN_IO_BAD_EVENT_TYPE", "Unknown timeline event type: " + s}
    });
}

common::ErrorList plan_io_error(std::string code, std::string message) {
    return common::ErrorList{{common::ErrorCategory::Planning, std::move(code), std::move(message)}};
}

struct PlanRenderWindow {
    std::string emitter_id;
    uint32_t channel{0};
    double start_sec{0.0};
    double end_sec{0.0};
};

struct PlanMixWindow {
    std::string label;
    uint32_t channel{0};
    double start_sec{0.0};
    double end_sec{0.0};
    std::vector<std::string> emitter_ids;
};

std::string channel_binding_key(const std::string& device_id, uint32_t channel_index) {
    return device_id + ":" + std::to_string(channel_index);
}

std::unordered_map<std::string, std::string> expected_channel_ids_by_binding(const Scenario& scenario) {
    std::unordered_map<std::string, std::string> channels;
    if (!scenario.channel_defs.empty()) {
        for (const auto& channel : scenario.channel_defs) {
            channels[channel_binding_key(channel.device, channel.index)] = channel.id;
        }
        return channels;
    }

    for (const auto& device : scenario.devices) {
        const auto channel_index = device.channel.value_or(0);
        channels[channel_binding_key(device.id, channel_index)] =
            device.id + "_ch" + std::to_string(channel_index);
    }
    return channels;
}

const ChannelPlan* find_channel_plan_by_index(const Plan& plan, uint32_t channel) {
    auto it = std::find_if(plan.channel_plans.begin(), plan.channel_plans.end(),
                           [channel](const ChannelPlan& cp) {
                               return cp.channel_index == channel;
                           });
    return it != plan.channel_plans.end() ? &*it : nullptr;
}

std::vector<uint32_t> active_channel_indices(const Plan& plan) {
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

bool mix_group_contains(const MixGroup& group, const std::string& emitter_id) {
    return std::find(group.emitter_ids.begin(), group.emitter_ids.end(), emitter_id) !=
           group.emitter_ids.end();
}

bool same_mix_group_covers_overlap(const Plan& plan,
                                   const PlanRenderWindow& a,
                                   const PlanRenderWindow& b) {
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

std::expected<void, common::ErrorList> validate_plan_schedule_windows(const Plan& plan) {
    const auto channels = active_channel_indices(plan);
    if (channels.empty()) {
        return {};
    }

    std::vector<PlanRenderWindow> windows;
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
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_INVALID_SCHEDULE_WINDOW",
                "render_instructions contains non-finite schedule window for emitter_id: '" +
                    a.emitter_id + "'"));
        }
        for (size_t j = i + 1; j < windows.size(); ++j) {
            const auto& b = windows[j];
            if (a.channel != b.channel ||
                !intervals_overlap(a.start_sec, a.end_sec, b.start_sec, b.end_sec) ||
                same_mix_group_covers_overlap(plan, a, b)) {
                continue;
            }
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_OVERLAPPING_RENDER_INSTRUCTIONS",
                "render_instructions emitter_id values '" + a.emitter_id + "' and '" +
                    b.emitter_id + "' overlap on channel " + std::to_string(a.channel) +
                    " without a covering mix group"));
        }
    }

    std::vector<PlanMixWindow> mix_windows;
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
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_INVALID_SCHEDULE_WINDOW",
                "mix_groups contains non-finite schedule window for device/channel: '" +
                    a.label + "'"));
        }
        for (size_t j = i + 1; j < mix_windows.size(); ++j) {
            const auto& b = mix_windows[j];
            if (a.channel == b.channel &&
                intervals_overlap(a.start_sec, a.end_sec, b.start_sec, b.end_sec)) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_OVERLAPPING_MIX_GROUPS",
                    "mix_groups overlap on channel " + std::to_string(a.channel)));
            }
        }
    }

    for (const auto& group : mix_windows) {
        for (const auto& window : windows) {
            if (group.channel != window.channel ||
                std::find(group.emitter_ids.begin(), group.emitter_ids.end(),
                          window.emitter_id) != group.emitter_ids.end() ||
                !intervals_overlap(group.start_sec, group.end_sec,
                                   window.start_sec, window.end_sec)) {
                continue;
            }
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_MIX_GROUP_OVERLAPS_RENDER",
                "mix_groups entry on channel " + std::to_string(group.channel) +
                    " overlaps render_instructions emitter_id '" + window.emitter_id +
                    "' outside the group"));
        }
    }

    return {};
}

std::expected<void, common::ErrorList> validate_waveform_params(const WaveformDef& waveform,
                                                                const std::string& field) {
    if (waveform.params.is_null()) {
        return {};
    }
    if (!waveform.params.is_object()) {
        return std::unexpected(plan_io_error(
            "E_PLAN_IO_BAD_WAVEFORM_PARAMS",
            field + ".params must be an object"));
    }
    if (waveform.params.contains("type")) {
        return std::unexpected(plan_io_error(
            "E_PLAN_IO_RESERVED_WAVEFORM_PARAM",
            field + ".params must not contain reserved key 'type'"));
    }
    return {};
}

std::expected<void, common::ErrorList> require_key(const nlohmann::json& j,
                                                   const char* key,
                                                   std::string field) {
    if (!j.contains(key)) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "." + key + "' is required"));
    }
    return {};
}

std::expected<void, common::ErrorList> require_object_payload(const nlohmann::json& payload,
                                                              std::string field) {
    if (payload.is_null()) {
        return {};
    }
    if (!payload.is_object()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be an object"));
    }
    return {};
}

std::expected<uint32_t, common::ErrorList> uint32_from_json(const nlohmann::json& value, std::string field) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be an unsigned 32-bit integer"));
    }

    if (value.is_number_integer()) {
        const auto parsed = value.get<std::int64_t>();
        if (parsed < 0) {
            return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                                 "'" + field + "' must be non-negative"));
        }
        if (parsed > static_cast<std::int64_t>(std::numeric_limits<uint32_t>::max())) {
            return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                                 "'" + field + "' exceeds uint32 range"));
        }
        return static_cast<uint32_t>(parsed);
    }

    const auto parsed = value.get<std::uint64_t>();
    if (parsed > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' exceeds uint32 range"));
    }
    return static_cast<uint32_t>(parsed);
}

std::expected<size_t, common::ErrorList> size_from_json(const nlohmann::json& value, std::string field) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be an unsigned integer"));
    }

    if (value.is_number_integer()) {
        const auto parsed = value.get<std::int64_t>();
        if (parsed < 0) {
            return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                                 "'" + field + "' must be non-negative"));
        }
        return static_cast<size_t>(parsed);
    }

    const auto parsed = value.get<std::uint64_t>();
    if (parsed > static_cast<std::uint64_t>(std::numeric_limits<size_t>::max())) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' exceeds size_t range"));
    }
    return static_cast<size_t>(parsed);
}

std::expected<int, common::ErrorList> non_negative_int_from_json(const nlohmann::json& value,
                                                                 std::string field) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be a non-negative integer"));
    }

    if (value.is_number_integer()) {
        const auto parsed = value.get<std::int64_t>();
        if (parsed < 0) {
            return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                                 "'" + field + "' must be non-negative"));
        }
        if (parsed > std::numeric_limits<int>::max()) {
            return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                                 "'" + field + "' exceeds int range"));
        }
        return static_cast<int>(parsed);
    }

    const auto parsed = value.get<std::uint64_t>();
    if (parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' exceeds int range"));
    }
    return static_cast<int>(parsed);
}

std::expected<std::vector<std::string>, common::ErrorList> string_array_from_json(const nlohmann::json& value,
                                                                                  std::string field) {
    if (!value.is_array()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be an array of strings"));
    }

    std::vector<std::string> result;
    result.reserve(value.size());
    for (const auto& item : value) {
        if (!item.is_string()) {
            return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                                 "'" + field + "' entries must be strings"));
        }
        result.push_back(item.get<std::string>());
    }
    return result;
}

std::expected<const nlohmann::json*, common::ErrorList> array_from_json(const nlohmann::json& value,
                                                                        std::string field) {
    if (!value.is_array()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be an array"));
    }
    return &value;
}

std::expected<const nlohmann::json*, common::ErrorList> object_from_json(const nlohmann::json& value,
                                                                         std::string field) {
    if (!value.is_object()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be an object"));
    }
    return &value;
}

std::expected<std::string, common::ErrorList> string_from_json(const nlohmann::json& value,
                                                               std::string field) {
    if (!value.is_string()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be a string"));
    }
    return value.get<std::string>();
}

std::expected<std::string, common::ErrorList> non_empty_string_from_json(const nlohmann::json& value,
                                                                         std::string field) {
    auto parsed = string_from_json(value, field);
    if (!parsed.has_value()) return parsed;
    if (parsed->empty()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' must be non-empty"));
    }
    return parsed;
}

std::expected<double, common::ErrorList> double_from_json(const nlohmann::json& value,
                                                          std::string field) {
    if (!value.is_number()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be a finite number"));
    }
    const double parsed = value.get<double>();
    if (!std::isfinite(parsed)) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' must be finite"));
    }
    return parsed;
}

std::expected<double, common::ErrorList> non_negative_double_from_json(const nlohmann::json& value,
                                                                       std::string field) {
    auto parsed = double_from_json(value, field);
    if (!parsed.has_value()) return parsed;
    if (*parsed < 0.0) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' must be non-negative"));
    }
    return parsed;
}

std::expected<double, common::ErrorList> positive_double_from_json(const nlohmann::json& value,
                                                                   std::string field) {
    auto parsed = double_from_json(value, field);
    if (!parsed.has_value()) return parsed;
    if (*parsed <= 0.0) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' must be positive"));
    }
    return parsed;
}

std::expected<bool, common::ErrorList> bool_from_json(const nlohmann::json& value,
                                                      std::string field) {
    if (!value.is_boolean()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             "'" + field + "' has invalid type; must be a boolean"));
    }
    return value.get<bool>();
}

std::expected<void, common::ErrorList> require_timeline_payload_key(const TimelineEvent& event,
                                                                    const char* key) {
    if (!event.payload.contains(key)) {
        return std::unexpected(plan_io_error(
            "E_PLAN_IO_BAD_TIMELINE_PAYLOAD",
            std::string("timeline payload for ") + timeline_event_type_to_string(event.type) +
                " is missing required key '" + key + "'"));
    }
    return {};
}

std::expected<void, common::ErrorList> validate_timeline_payload_shape(const TimelineEvent& event) {
    switch (event.type) {
        case TimelineEventType::EmitterStart:
        case TimelineEventType::EmitterStop:
            return {};
        case TimelineEventType::FreqChange: {
            auto required = require_timeline_payload_key(event, "freq_hz");
            if (!required.has_value()) return required;
            auto freq = positive_double_from_json(event.payload["freq_hz"],
                                                  "timeline.payload.freq_hz");
            if (!freq.has_value()) return std::unexpected(std::move(freq).error());
            return {};
        }
        case TimelineEventType::GainChange: {
            auto required = require_timeline_payload_key(event, "gain_db");
            if (!required.has_value()) return required;
            auto gain = double_from_json(event.payload["gain_db"], "timeline.payload.gain_db");
            if (!gain.has_value()) return std::unexpected(std::move(gain).error());
            return {};
        }
        case TimelineEventType::Marker:
            if (event.payload.contains("name")) {
                auto name = non_empty_string_from_json(event.payload["name"], "timeline.payload.name");
                if (!name.has_value()) return std::unexpected(std::move(name).error());
            }
            return {};
        case TimelineEventType::WaveformSwitch: {
            auto required_emitter = require_timeline_payload_key(event, "emitter_id");
            if (!required_emitter.has_value()) return required_emitter;
            auto emitter_id = non_empty_string_from_json(event.payload["emitter_id"],
                                                         "timeline.payload.emitter_id");
            if (!emitter_id.has_value()) return std::unexpected(std::move(emitter_id).error());
            auto required_waveform = require_timeline_payload_key(event, "new_waveform");
            if (!required_waveform.has_value()) return required_waveform;
            auto new_waveform = non_empty_string_from_json(event.payload["new_waveform"],
                                                           "timeline.payload.new_waveform");
            if (!new_waveform.has_value()) return std::unexpected(std::move(new_waveform).error());
            return {};
        }
        case TimelineEventType::ImpairmentChange: {
            auto required_emitter = require_timeline_payload_key(event, "emitter_id");
            if (!required_emitter.has_value()) return required_emitter;
            auto emitter_id = non_empty_string_from_json(event.payload["emitter_id"],
                                                         "timeline.payload.emitter_id");
            if (!emitter_id.has_value()) return std::unexpected(std::move(emitter_id).error());
            auto required_impairment = require_timeline_payload_key(event, "impairment");
            if (!required_impairment.has_value()) return required_impairment;
            auto impairment = non_empty_string_from_json(event.payload["impairment"],
                                                         "timeline.payload.impairment");
            if (!impairment.has_value()) return std::unexpected(std::move(impairment).error());
            if (event.payload.contains("enabled")) {
                auto enabled = bool_from_json(event.payload["enabled"], "timeline.payload.enabled");
                if (!enabled.has_value()) return std::unexpected(std::move(enabled).error());
            }
            return {};
        }
    }
    return {};
}

std::expected<MixingMode, common::ErrorList> mixing_mode_from_string(const std::string& s) {
    if (s == "additive") return MixingMode::Additive;
    if (s == "none") return MixingMode::None;
    return std::unexpected(plan_io_error("E_PLAN_IO_BAD_MIXING_MODE",
                                         "Unknown emitter mixing mode in plan JSON: " + s));
}

std::expected<RunMode, common::ErrorList> run_mode_from_string(const std::string& s) {
    if (s == "replay") return RunMode::Replay;
    if (s == "realtime") return RunMode::Realtime;
    return std::unexpected(plan_io_error("E_PLAN_IO_BAD_RUN_MODE",
                                         "Unknown run mode in plan JSON: " + s));
}

bool valid_sync_group_mode(const std::string& s) {
    return s == "coherent" || s == "independent";
}

std::expected<common::ErrorCategory, common::ErrorList> error_category_from_string(const std::string& s,
                                                                                   std::string field) {
    if (s == "Config") return common::ErrorCategory::Config;
    if (s == "Validation") return common::ErrorCategory::Validation;
    if (s == "Planning") return common::ErrorCategory::Planning;
    if (s == "Preparation") return common::ErrorCategory::Preparation;
    if (s == "Execution") return common::ErrorCategory::Execution;
    if (s == "QualityWarning") return common::ErrorCategory::QualityWarning;
    return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                         "'" + field + "' has unknown category: " + s));
}

std::expected<common::Error, common::ErrorList> diagnostic_from_json(const nlohmann::json& j,
                                                                     std::string field) {
    auto obj = object_from_json(j, field);
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    auto category_it = j.find("category");
    if (category_it == j.end()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON", "'" + field + ".category' is required"));
    }
    auto category_text = string_from_json(*category_it, field + ".category");
    if (!category_text.has_value()) return std::unexpected(std::move(category_text).error());
    auto category = error_category_from_string(*category_text, field + ".category");
    if (!category.has_value()) return std::unexpected(std::move(category).error());

    auto code_it = j.find("code");
    if (code_it == j.end()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON", "'" + field + ".code' is required"));
    }
    auto code = non_empty_string_from_json(*code_it, field + ".code");
    if (!code.has_value()) return std::unexpected(std::move(code).error());
    auto message_it = j.find("message");
    if (message_it == j.end()) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON", "'" + field + ".message' is required"));
    }
    auto message = non_empty_string_from_json(*message_it, field + ".message");
    if (!message.has_value()) return std::unexpected(std::move(message).error());

    return common::Error{*category, *code, *message};
}

} // namespace

nlohmann::json rf_settings_to_json(const RfSettings& rf) {
    nlohmann::json j;
    j["freq_hz"] = rf.freq_hz;
    j["rate_sps"] = rf.rate_sps;
    j["gain_db"] = rf.gain_db;
    if (rf.bandwidth_hz.has_value()) j["bandwidth_hz"] = *rf.bandwidth_hz;
    if (rf.antenna.has_value()) j["antenna"] = *rf.antenna;
    return j;
}

std::expected<RfSettings, common::ErrorList> rf_settings_from_json(const nlohmann::json& j) {
    auto obj = object_from_json(j, "rf");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"freq_hz", "rate_sps", "gain_db"}) {
        auto required = require_key(j, key, "rf");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    RfSettings rf;
    if (j.contains("freq_hz")) {
        auto freq_hz = positive_double_from_json(j["freq_hz"], "rf.freq_hz");
        if (!freq_hz.has_value()) return std::unexpected(std::move(freq_hz).error());
        rf.freq_hz = *freq_hz;
    }
    if (j.contains("rate_sps")) {
        auto rate_sps = positive_double_from_json(j["rate_sps"], "rf.rate_sps");
        if (!rate_sps.has_value()) return std::unexpected(std::move(rate_sps).error());
        rf.rate_sps = *rate_sps;
    }
    if (j.contains("gain_db")) {
        auto gain_db = double_from_json(j["gain_db"], "rf.gain_db");
        if (!gain_db.has_value()) return std::unexpected(std::move(gain_db).error());
        rf.gain_db = *gain_db;
    }
    if (j.contains("bandwidth_hz")) {
        auto bandwidth_hz = positive_double_from_json(j["bandwidth_hz"], "rf.bandwidth_hz");
        if (!bandwidth_hz.has_value()) return std::unexpected(std::move(bandwidth_hz).error());
        rf.bandwidth_hz = *bandwidth_hz;
    }
    if (j.contains("antenna")) {
        auto antenna = string_from_json(j["antenna"], "rf.antenna");
        if (!antenna.has_value()) return std::unexpected(std::move(antenna).error());
        rf.antenna = *antenna;
    }
    return rf;
}

nlohmann::json waveform_def_to_json(const WaveformDef& wf) {
    nlohmann::json j;
    if (wf.id.has_value()) j["id"] = *wf.id;
    j["type"] = dsp::to_string(wf.type);
    if (wf.target_power_dbm.has_value()) j["target_power_dbm"] = *wf.target_power_dbm;
    j["params"] = wf.params.is_null() ? nlohmann::json::object() : wf.params;
    return j;
}

std::expected<WaveformDef, common::ErrorList> waveform_def_from_json(const nlohmann::json& j) {
    auto obj = object_from_json(j, "waveform");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    auto required_type = require_key(j, "type", "waveform");
    if (!required_type.has_value()) return std::unexpected(std::move(required_type).error());

    WaveformDef wf;
    if (j.contains("id")) {
        auto id = string_from_json(j["id"], "waveform.id");
        if (!id.has_value()) return std::unexpected(std::move(id).error());
        wf.id = *id;
    }
    if (j.contains("type")) {
        auto type_str = string_from_json(j["type"], "waveform.type");
        if (!type_str.has_value()) return std::unexpected(std::move(type_str).error());
        auto type_result = dsp::waveform_type_from_string(*type_str);
        if (!type_result.has_value()) {
            return std::unexpected(common::ErrorList{
                {common::ErrorCategory::Planning, "E_PLAN_IO_BAD_WAVEFORM_TYPE",
                 "Unknown waveform type in plan JSON: " + *type_str}});
        }
        wf.type = *type_result;
    }
    wf.params = nlohmann::json::object();
    if (j.contains("params") && !j["params"].is_null()) {
        auto params = object_from_json(j["params"], "waveform.params");
        if (!params.has_value()) return std::unexpected(std::move(params).error());
        if ((*params)->contains("type")) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_RESERVED_WAVEFORM_PARAM",
                "waveform.params must not contain reserved key 'type'"));
        }
        wf.params = **params;
    }
    if (j.contains("target_power_dbm")) {
        auto target_power_dbm = double_from_json(j["target_power_dbm"], "waveform.target_power_dbm");
        if (!target_power_dbm.has_value()) return std::unexpected(std::move(target_power_dbm).error());
        wf.target_power_dbm = *target_power_dbm;
    }
    return wf;
}

nlohmann::json impairment_settings_to_json(const ImpairmentSettings& imp) {
    nlohmann::json ij;
    if (imp.cfo_hz.has_value()) ij["cfo_hz"] = *imp.cfo_hz;
    if (imp.phase_offset_rad.has_value()) ij["phase_offset_rad"] = *imp.phase_offset_rad;
    if (imp.iq_gain_imbalance_db.has_value()) ij["iq_gain_imbalance_db"] = *imp.iq_gain_imbalance_db;
    if (imp.iq_phase_imbalance_rad.has_value()) ij["iq_phase_imbalance_rad"] = *imp.iq_phase_imbalance_rad;
    if (imp.dc_offset_i.has_value()) ij["dc_offset_i"] = *imp.dc_offset_i;
    if (imp.dc_offset_q.has_value()) ij["dc_offset_q"] = *imp.dc_offset_q;
    if (imp.awgn_power.has_value()) ij["awgn_power"] = *imp.awgn_power;
    if (imp.amplitude_ripple_db.has_value()) ij["amplitude_ripple_db"] = *imp.amplitude_ripple_db;
    if (imp.amplitude_ripple_freq_hz.has_value()) ij["amplitude_ripple_freq_hz"] = *imp.amplitude_ripple_freq_hz;
    if (imp.delay_sec.has_value()) ij["delay_sec"] = *imp.delay_sec;
    if (imp.burst_dropout_rate.has_value()) ij["burst_dropout_rate"] = *imp.burst_dropout_rate;
    if (imp.burst_dropout_mean_burst_sec.has_value()) ij["burst_dropout_mean_burst_sec"] = *imp.burst_dropout_mean_burst_sec;
    if (imp.phase_noise_bandwidth_hz.has_value()) ij["phase_noise_bandwidth_hz"] = *imp.phase_noise_bandwidth_hz;
    if (imp.phase_noise_magnitude_rad.has_value()) ij["phase_noise_magnitude_rad"] = *imp.phase_noise_magnitude_rad;
    if (imp.phase_noise_psd_shape.has_value()) ij["phase_noise_psd_shape"] = *imp.phase_noise_psd_shape;
    if (imp.multipath_delay_samples.has_value()) ij["multipath_delay_samples"] = *imp.multipath_delay_samples;
    if (imp.multipath_amplitude.has_value()) ij["multipath_amplitude"] = *imp.multipath_amplitude;
    if (imp.fading_doppler_hz.has_value()) ij["fading_doppler_hz"] = *imp.fading_doppler_hz;
    if (imp.fading_type.has_value()) ij["fading_type"] = *imp.fading_type;
    if (imp.fading_k_factor.has_value()) ij["fading_k_factor"] = *imp.fading_k_factor;
    if (imp.pa_model.has_value()) ij["pa_model"] = *imp.pa_model;
    if (imp.pa_saturation.has_value()) ij["pa_saturation"] = *imp.pa_saturation;
    if (imp.pa_smoothness.has_value()) ij["pa_smoothness"] = *imp.pa_smoothness;
    if (imp.pa_phase_shift.has_value()) ij["pa_phase_shift"] = *imp.pa_phase_shift;
    return ij;
}

std::expected<ImpairmentSettings, common::ErrorList> impairment_settings_from_json(const nlohmann::json& ij) {
    auto obj = object_from_json(ij, "impairments");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    ImpairmentSettings imp;
    auto set_double = [&](const char* key, std::optional<double>& out) -> std::optional<common::ErrorList> {
        if (!ij.contains(key)) return std::nullopt;
        auto value = double_from_json(ij[key], std::string("impairments.") + key);
        if (!value.has_value()) return std::move(value).error();
        out = *value;
        return std::nullopt;
    };
    auto set_string = [&](const char* key, std::optional<std::string>& out) -> std::optional<common::ErrorList> {
        if (!ij.contains(key)) return std::nullopt;
        auto value = string_from_json(ij[key], std::string("impairments.") + key);
        if (!value.has_value()) return std::move(value).error();
        out = *value;
        return std::nullopt;
    };

    if (auto error = set_double("cfo_hz", imp.cfo_hz)) return std::unexpected(std::move(*error));
    if (auto error = set_double("phase_offset_rad", imp.phase_offset_rad)) return std::unexpected(std::move(*error));
    if (auto error = set_double("iq_gain_imbalance_db", imp.iq_gain_imbalance_db)) return std::unexpected(std::move(*error));
    if (auto error = set_double("iq_phase_imbalance_rad", imp.iq_phase_imbalance_rad)) return std::unexpected(std::move(*error));
    if (auto error = set_double("dc_offset_i", imp.dc_offset_i)) return std::unexpected(std::move(*error));
    if (auto error = set_double("dc_offset_q", imp.dc_offset_q)) return std::unexpected(std::move(*error));
    if (auto error = set_double("awgn_power", imp.awgn_power)) return std::unexpected(std::move(*error));
    if (auto error = set_double("amplitude_ripple_db", imp.amplitude_ripple_db)) return std::unexpected(std::move(*error));
    if (auto error = set_double("amplitude_ripple_freq_hz", imp.amplitude_ripple_freq_hz)) return std::unexpected(std::move(*error));
    if (auto error = set_double("delay_sec", imp.delay_sec)) return std::unexpected(std::move(*error));
    if (auto error = set_double("burst_dropout_rate", imp.burst_dropout_rate)) return std::unexpected(std::move(*error));
    if (auto error = set_double("burst_dropout_mean_burst_sec", imp.burst_dropout_mean_burst_sec)) return std::unexpected(std::move(*error));
    if (auto error = set_double("phase_noise_bandwidth_hz", imp.phase_noise_bandwidth_hz)) return std::unexpected(std::move(*error));
    if (auto error = set_double("phase_noise_magnitude_rad", imp.phase_noise_magnitude_rad)) return std::unexpected(std::move(*error));
    if (auto error = set_string("phase_noise_psd_shape", imp.phase_noise_psd_shape)) return std::unexpected(std::move(*error));
    if (auto error = set_double("multipath_delay_samples", imp.multipath_delay_samples)) return std::unexpected(std::move(*error));
    if (auto error = set_double("multipath_amplitude", imp.multipath_amplitude)) return std::unexpected(std::move(*error));
    if (auto error = set_double("fading_doppler_hz", imp.fading_doppler_hz)) return std::unexpected(std::move(*error));
    if (auto error = set_string("fading_type", imp.fading_type)) return std::unexpected(std::move(*error));
    if (auto error = set_double("fading_k_factor", imp.fading_k_factor)) return std::unexpected(std::move(*error));
    if (auto error = set_string("pa_model", imp.pa_model)) return std::unexpected(std::move(*error));
    if (auto error = set_double("pa_saturation", imp.pa_saturation)) return std::unexpected(std::move(*error));
    if (auto error = set_double("pa_smoothness", imp.pa_smoothness)) return std::unexpected(std::move(*error));
    if (auto error = set_double("pa_phase_shift", imp.pa_phase_shift)) return std::unexpected(std::move(*error));
    return imp;
}

nlohmann::json metadata_to_json(const Metadata& m) {
    nlohmann::json j;
    j["name"] = m.name;
    if (m.description.has_value()) j["description"] = *m.description;
    if (m.version.has_value()) j["version"] = *m.version;
    return j;
}

std::expected<Metadata, common::ErrorList> metadata_from_json(const nlohmann::json& j) {
    auto obj = object_from_json(j, "metadata");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    auto required_name = require_key(j, "name", "metadata");
    if (!required_name.has_value()) return std::unexpected(std::move(required_name).error());

    Metadata m;
    if (j.contains("name")) {
        auto name = string_from_json(j["name"], "metadata.name");
        if (!name.has_value()) return std::unexpected(std::move(name).error());
        m.name = *name;
    }
    if (j.contains("description")) {
        auto description = string_from_json(j["description"], "metadata.description");
        if (!description.has_value()) return std::unexpected(std::move(description).error());
        m.description = *description;
    }
    if (j.contains("version")) {
        auto version = string_from_json(j["version"], "metadata.version");
        if (!version.has_value()) return std::unexpected(std::move(version).error());
        m.version = *version;
    }
    return m;
}

nlohmann::json device_def_to_json(const DeviceDef& d) {
    nlohmann::json j;
    j["id"] = d.id;
    if (d.channel.has_value()) j["channel"] = *d.channel;
    j["rf"] = rf_settings_to_json(d.rf);
    return j;
}

std::expected<DeviceDef, common::ErrorList> device_def_from_json(const nlohmann::json& j) {
    auto obj = object_from_json(j, "device");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    auto required_id = require_key(j, "id", "device");
    if (!required_id.has_value()) return std::unexpected(std::move(required_id).error());

    DeviceDef d;
    if (j.contains("id")) {
        auto id = string_from_json(j["id"], "device.id");
        if (!id.has_value()) return std::unexpected(std::move(id).error());
        d.id = *id;
    }
    if (j.contains("channel")) {
        auto channel = uint32_from_json(j["channel"], "device.channel");
        if (!channel.has_value()) return std::unexpected(std::move(channel).error());
        d.channel = *channel;
    }
    auto required_rf = require_key(j, "rf", "device");
    if (!required_rf.has_value()) return std::unexpected(std::move(required_rf).error());
    auto rf = rf_settings_from_json(j["rf"]);
    if (!rf.has_value()) return std::unexpected(std::move(rf).error());
    d.rf = *rf;
    return d;
}

nlohmann::json emitter_def_to_json(const EmitterDef& e) {
    nlohmann::json j;
    j["id"] = e.id;
    j["device"] = e.device;
    j["channel"] = e.channel;
    j["start_after_sec"] = e.start_after_sec;
    j["duration_sec"] = e.duration_sec;
    if (e.waveform.has_value()) j["waveform"] = waveform_def_to_json(*e.waveform);
    if (e.waveform_ref.has_value()) j["waveform_ref"] = *e.waveform_ref;
    if (e.channel_id.has_value()) j["channel_id"] = *e.channel_id;
    if (e.mixing == MixingMode::Additive) {
        j["mixing"] = "additive";
    } else {
        j["mixing"] = "none";
    }
    if (e.repeat.has_value()) {
        nlohmann::json rj;
        rj["count"] = e.repeat->count;
        rj["interval_sec"] = e.repeat->interval_sec;
        j["repeat"] = rj;
    }
    if (e.impairments.has_value()) {
        j["impairments"] = impairment_settings_to_json(*e.impairments);
    }
    return j;
}

std::expected<EmitterDef, common::ErrorList> emitter_def_from_json(const nlohmann::json& j) {
    auto obj = object_from_json(j, "emitter");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"id", "device", "channel", "start_after_sec", "duration_sec", "mixing"}) {
        auto required = require_key(j, key, "emitter");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    EmitterDef e;
    if (j.contains("id")) {
        auto id = string_from_json(j["id"], "emitter.id");
        if (!id.has_value()) return std::unexpected(std::move(id).error());
        e.id = *id;
    }
    if (j.contains("device")) {
        auto device = string_from_json(j["device"], "emitter.device");
        if (!device.has_value()) return std::unexpected(std::move(device).error());
        e.device = *device;
    }
    if (j.contains("channel")) {
        auto channel = uint32_from_json(j["channel"], "emitter.channel");
        if (!channel.has_value()) return std::unexpected(std::move(channel).error());
        e.channel = *channel;
    }
    if (j.contains("start_after_sec")) {
        auto start_after_sec = non_negative_double_from_json(j["start_after_sec"], "emitter.start_after_sec");
        if (!start_after_sec.has_value()) return std::unexpected(std::move(start_after_sec).error());
        e.start_after_sec = *start_after_sec;
    }
    if (j.contains("duration_sec")) {
        auto duration_sec = non_negative_double_from_json(j["duration_sec"], "emitter.duration_sec");
        if (!duration_sec.has_value()) return std::unexpected(std::move(duration_sec).error());
        e.duration_sec = *duration_sec;
    }
    if (j.contains("waveform")) {
        auto wf = waveform_def_from_json(j["waveform"]);
        if (!wf.has_value()) return std::unexpected(std::move(wf).error());
        e.waveform = *wf;
    }
    if (j.contains("waveform_ref")) {
        auto waveform_ref = string_from_json(j["waveform_ref"], "emitter.waveform_ref");
        if (!waveform_ref.has_value()) return std::unexpected(std::move(waveform_ref).error());
        e.waveform_ref = *waveform_ref;
    }
    if (j.contains("channel_id")) {
        auto channel_id = string_from_json(j["channel_id"], "emitter.channel_id");
        if (!channel_id.has_value()) return std::unexpected(std::move(channel_id).error());
        e.channel_id = *channel_id;
    }
    if (j.contains("mixing")) {
        auto m = string_from_json(j["mixing"], "emitter.mixing");
        if (!m.has_value()) return std::unexpected(std::move(m).error());
        auto mode = mixing_mode_from_string(*m);
        if (!mode.has_value()) return std::unexpected(std::move(mode).error());
        e.mixing = *mode;
    }
    if (j.contains("repeat")) {
        auto repeat_obj = object_from_json(j["repeat"], "emitter.repeat");
        if (!repeat_obj.has_value()) return std::unexpected(std::move(repeat_obj).error());
        RepeatSpec rs;
        if (j["repeat"].contains("count")) {
            auto count = non_negative_int_from_json(j["repeat"]["count"], "emitter.repeat.count");
            if (!count.has_value()) return std::unexpected(std::move(count).error());
            rs.count = *count;
        }
        if (j["repeat"].contains("interval_sec")) {
            auto interval_sec = non_negative_double_from_json(j["repeat"]["interval_sec"],
                                                             "emitter.repeat.interval_sec");
            if (!interval_sec.has_value()) return std::unexpected(std::move(interval_sec).error());
            rs.interval_sec = *interval_sec;
        }
        e.repeat = rs;
    }
    if (j.contains("impairments")) {
        auto imp = impairment_settings_from_json(j["impairments"]);
        if (!imp.has_value()) return std::unexpected(std::move(imp).error());
        e.impairments = *imp;
    }
    return e;
}

nlohmann::json reporting_config_to_json(const ReportingConfig& r) {
    return nlohmann::json{{"save_plan", r.save_plan}, {"save_metrics", r.save_metrics}};
}

std::expected<ReportingConfig, common::ErrorList> reporting_config_from_json(const nlohmann::json& j) {
    auto obj = object_from_json(j, "reporting");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"save_plan", "save_metrics"}) {
        auto required = require_key(j, key, "reporting");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    ReportingConfig r;
    if (j.contains("save_plan")) {
        auto save_plan = bool_from_json(j["save_plan"], "reporting.save_plan");
        if (!save_plan.has_value()) return std::unexpected(std::move(save_plan).error());
        r.save_plan = *save_plan;
    }
    if (j.contains("save_metrics")) {
        auto save_metrics = bool_from_json(j["save_metrics"], "reporting.save_metrics");
        if (!save_metrics.has_value()) return std::unexpected(std::move(save_metrics).error());
        r.save_metrics = *save_metrics;
    }
    return r;
}

nlohmann::json channel_def_to_json(const ChannelDef& c) {
    nlohmann::json j;
    j["id"] = c.id;
    j["device"] = c.device;
    j["index"] = c.index;
    j["rf"] = rf_settings_to_json(c.rf);
    return j;
}

std::expected<ChannelDef, common::ErrorList> channel_def_from_json(const nlohmann::json& j) {
    auto obj = object_from_json(j, "channel");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"id", "device", "index"}) {
        auto required = require_key(j, key, "channel");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    ChannelDef c;
    if (j.contains("id")) {
        auto id = string_from_json(j["id"], "channel.id");
        if (!id.has_value()) return std::unexpected(std::move(id).error());
        c.id = *id;
    }
    if (j.contains("device")) {
        auto device = string_from_json(j["device"], "channel.device");
        if (!device.has_value()) return std::unexpected(std::move(device).error());
        c.device = *device;
    }
    if (j.contains("index")) {
        auto index = uint32_from_json(j["index"], "channel.index");
        if (!index.has_value()) return std::unexpected(std::move(index).error());
        c.index = *index;
    }
    auto required_rf = require_key(j, "rf", "channel");
    if (!required_rf.has_value()) return std::unexpected(std::move(required_rf).error());
    auto rf = rf_settings_from_json(j["rf"]);
    if (!rf.has_value()) return std::unexpected(std::move(rf).error());
    c.rf = *rf;
    return c;
}

nlohmann::json sync_group_to_json(const SyncGroup& sg) {
    nlohmann::json j;
    j["id"] = sg.id;
    j["channels"] = sg.channels;
    j["mode"] = sg.mode;
    return j;
}

std::expected<SyncGroup, common::ErrorList> sync_group_from_json(const nlohmann::json& j) {
    auto obj = object_from_json(j, "sync_group");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"id", "mode", "channels"}) {
        auto required = require_key(j, key, "sync_group");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    SyncGroup sg;
    if (j.contains("id")) {
        auto id = string_from_json(j["id"], "sync_groups.id");
        if (!id.has_value()) return std::unexpected(std::move(id).error());
        sg.id = *id;
    }
    if (j.contains("mode")) {
        auto mode = string_from_json(j["mode"], "sync_groups.mode");
        if (!mode.has_value()) return std::unexpected(std::move(mode).error());
        if (!valid_sync_group_mode(*mode)) {
            return std::unexpected(plan_io_error("E_PLAN_IO_BAD_SYNC_MODE",
                                                 "Unknown sync group mode in plan JSON: " + *mode));
        }
        sg.mode = *mode;
    }
    if (j.contains("channels")) {
        auto channels = string_array_from_json(j["channels"], "sync_groups.channels");
        if (!channels.has_value()) return std::unexpected(std::move(channels).error());
        sg.channels = std::move(*channels);
    }
    return sg;
}

nlohmann::json scenario_to_json(const Scenario& s) {
    nlohmann::json j;
    j["metadata"] = metadata_to_json(s.metadata);
    for (const auto& d : s.devices) j["devices"].push_back(device_def_to_json(d));
    for (const auto& w : s.waveforms) j["waveforms"].push_back(waveform_def_to_json(w));
    for (const auto& e : s.emitters) j["emitters"].push_back(emitter_def_to_json(e));
    j["reporting"] = reporting_config_to_json(s.reporting);
    for (const auto& c : s.channel_defs) j["channels"].push_back(channel_def_to_json(c));
    for (const auto& sg : s.sync_groups) j["sync_groups"].push_back(sync_group_to_json(sg));
    for (const auto& evt : s.events) {
        nlohmann::json ej;
        ej["target_device"] = evt.target_device;
        ej["time_sec"] = evt.time_sec;
        ej["type"] = evt.type;
        ej["payload"] = evt.payload.is_null() ? nlohmann::json::object() : evt.payload;
        j["events"].push_back(ej);
    }
    {
        nlohmann::json rj;
        rj["mode"] = (s.run.mode == RunMode::Replay) ? "replay" : "realtime";
        j["run"] = rj;
    }
    return j;
}

std::expected<Scenario, common::ErrorList> scenario_from_json(const nlohmann::json& j) {
    try {
    auto obj = object_from_json(j, "scenario");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"metadata", "devices", "emitters", "reporting", "run"}) {
        auto required = require_key(j, key, "scenario");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    Scenario s;
    if (j.contains("metadata")) {
        auto m = metadata_from_json(j["metadata"]);
        if (!m.has_value()) return std::unexpected(std::move(m).error());
        s.metadata = *m;
    }
    if (j.contains("devices")) {
        auto devices = array_from_json(j["devices"], "devices");
        if (!devices.has_value()) return std::unexpected(std::move(devices).error());
        for (const auto& dj : **devices) {
            auto d = device_def_from_json(dj);
            if (!d.has_value()) return std::unexpected(std::move(d).error());
            s.devices.push_back(*d);
        }
    }
    if (j.contains("waveforms")) {
        auto waveforms = array_from_json(j["waveforms"], "waveforms");
        if (!waveforms.has_value()) return std::unexpected(std::move(waveforms).error());
        for (const auto& wj : **waveforms) {
            auto w = waveform_def_from_json(wj);
            if (!w.has_value()) return std::unexpected(std::move(w).error());
            s.waveforms.push_back(*w);
        }
    }
    if (j.contains("emitters")) {
        auto emitters = array_from_json(j["emitters"], "emitters");
        if (!emitters.has_value()) return std::unexpected(std::move(emitters).error());
        for (const auto& ej : **emitters) {
            auto e = emitter_def_from_json(ej);
            if (!e.has_value()) return std::unexpected(std::move(e).error());
            s.emitters.push_back(*e);
        }
    }
    if (j.contains("reporting")) {
        auto reporting = reporting_config_from_json(j["reporting"]);
        if (!reporting.has_value()) return std::unexpected(std::move(reporting).error());
        s.reporting = *reporting;
    }
    if (j.contains("channels")) {
        auto channels = array_from_json(j["channels"], "channels");
        if (!channels.has_value()) return std::unexpected(std::move(channels).error());
        for (const auto& cj : **channels) {
            auto c = channel_def_from_json(cj);
            if (!c.has_value()) return std::unexpected(std::move(c).error());
            s.channel_defs.push_back(*c);
        }
    }
    if (j.contains("sync_groups")) {
        auto sync_groups = array_from_json(j["sync_groups"], "sync_groups");
        if (!sync_groups.has_value()) return std::unexpected(std::move(sync_groups).error());
        for (const auto& sgj : **sync_groups) {
            auto sg = sync_group_from_json(sgj);
            if (!sg.has_value()) return std::unexpected(std::move(sg).error());
            s.sync_groups.push_back(*sg);
        }
    }
    if (j.contains("events")) {
        auto events = array_from_json(j["events"], "events");
        if (!events.has_value()) return std::unexpected(std::move(events).error());
        for (const auto& ej : **events) {
            auto event_obj = object_from_json(ej, "events");
            if (!event_obj.has_value()) return std::unexpected(std::move(event_obj).error());
            for (const char* key : {"target_device", "time_sec", "type"}) {
                auto required = require_key(ej, key, "events");
                if (!required.has_value()) return std::unexpected(std::move(required).error());
            }
            ScenarioEvent evt;
            if (ej.contains("target_device")) {
                auto target_device = string_from_json(ej["target_device"], "events.target_device");
                if (!target_device.has_value()) return std::unexpected(std::move(target_device).error());
                evt.target_device = *target_device;
            }
            if (ej.contains("time_sec")) {
                auto time_sec = non_negative_double_from_json(ej["time_sec"], "events.time_sec");
                if (!time_sec.has_value()) return std::unexpected(std::move(time_sec).error());
                evt.time_sec = *time_sec;
            }
            if (ej.contains("type")) {
                auto type = string_from_json(ej["type"], "events.type");
                if (!type.has_value()) return std::unexpected(std::move(type).error());
                evt.type = *type;
            }
            if (ej.contains("payload")) {
                auto payload_obj = require_object_payload(ej["payload"], "events.payload");
                if (!payload_obj.has_value()) return std::unexpected(std::move(payload_obj).error());
                evt.payload = ej["payload"].is_null() ? nlohmann::json::object() : ej["payload"];
            }
            s.events.push_back(std::move(evt));
        }
    }
    if (j.contains("run")) {
        auto run_obj = object_from_json(j["run"], "run");
        if (!run_obj.has_value()) return std::unexpected(std::move(run_obj).error());
        auto required_mode = require_key(j["run"], "mode", "run");
        if (!required_mode.has_value()) return std::unexpected(std::move(required_mode).error());
        if (j["run"].contains("mode")) {
            auto mode = string_from_json(j["run"]["mode"], "run.mode");
            if (!mode.has_value()) return std::unexpected(std::move(mode).error());
            auto run_mode = run_mode_from_string(*mode);
            if (!run_mode.has_value()) return std::unexpected(std::move(run_mode).error());
            s.run.mode = *run_mode;
        }
    }
    return s;
    } catch (const nlohmann::json::exception& e) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             std::string("Invalid scenario JSON shape: ") + e.what()));
    }
}

nlohmann::json timeline_event_to_json(const TimelineEvent& ev) {
    nlohmann::json evj;
    evj["type"] = timeline_event_type_to_string(ev.type);
    evj["time_sec"] = ev.time_sec;
    evj["target_id"] = ev.target_id;
    evj["payload"] = ev.payload.is_null() ? nlohmann::json::object() : ev.payload;
    return evj;
}

std::expected<TimelineEvent, common::ErrorList> timeline_event_from_json(const nlohmann::json& evj) {
    auto obj = object_from_json(evj, "timeline");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"type", "time_sec", "target_id"}) {
        auto required = require_key(evj, key, "timeline");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    TimelineEvent ev;
    if (evj.contains("type")) {
        auto type_str = string_from_json(evj["type"], "timeline.type");
        if (!type_str.has_value()) return std::unexpected(std::move(type_str).error());
        auto type = string_to_timeline_event_type(*type_str);
        if (!type.has_value()) return std::unexpected(std::move(type).error());
        ev.type = *type;
    }
    if (evj.contains("time_sec")) {
        auto time = non_negative_double_from_json(evj["time_sec"], "timeline.time_sec");
        if (!time.has_value()) return std::unexpected(std::move(time).error());
        ev.time_sec = *time;
    }
    if (evj.contains("target_id")) {
        auto target_id = string_from_json(evj["target_id"], "timeline.target_id");
        if (!target_id.has_value()) return std::unexpected(std::move(target_id).error());
        ev.target_id = *target_id;
    }
    if (evj.contains("payload")) {
        auto payload_obj = require_object_payload(evj["payload"], "timeline.payload");
        if (!payload_obj.has_value()) return std::unexpected(std::move(payload_obj).error());
        ev.payload = evj["payload"].is_null() ? nlohmann::json::object() : evj["payload"];
    }
    return ev;
}

nlohmann::json render_instruction_to_json(const RenderInstruction& ri) {
    nlohmann::json rij;
    rij["emitter_id"] = ri.emitter_id;
    rij["waveform"] = waveform_def_to_json(ri.waveform);
    rij["start_sec"] = ri.start_sec;
    rij["duration_sec"] = ri.duration_sec;
    rij["sample_rate"] = ri.sample_rate;
    if (ri.resample_ratio.has_value()) rij["resample_ratio"] = *ri.resample_ratio;
    if (ri.impairments.has_value()) rij["impairments"] = impairment_settings_to_json(*ri.impairments);
    return rij;
}

std::expected<RenderInstruction, common::ErrorList> render_instruction_from_json(const nlohmann::json& rij) {
    auto obj = object_from_json(rij, "render_instructions");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"emitter_id", "waveform", "start_sec", "duration_sec", "sample_rate"}) {
        auto required = require_key(rij, key, "render_instructions");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    RenderInstruction ri;
    if (rij.contains("emitter_id")) {
        auto emitter_id = string_from_json(rij["emitter_id"], "render_instructions.emitter_id");
        if (!emitter_id.has_value()) return std::unexpected(std::move(emitter_id).error());
        ri.emitter_id = *emitter_id;
    }
    if (rij.contains("waveform")) {
        auto wf = waveform_def_from_json(rij["waveform"]);
        if (!wf.has_value()) return std::unexpected(std::move(wf).error());
        ri.waveform = *wf;
    }
    if (rij.contains("start_sec")) {
        auto start_sec = non_negative_double_from_json(rij["start_sec"], "render_instructions.start_sec");
        if (!start_sec.has_value()) return std::unexpected(std::move(start_sec).error());
        ri.start_sec = *start_sec;
    }
    if (rij.contains("duration_sec")) {
        auto duration_sec = positive_double_from_json(rij["duration_sec"],
                                                     "render_instructions.duration_sec");
        if (!duration_sec.has_value()) return std::unexpected(std::move(duration_sec).error());
        ri.duration_sec = *duration_sec;
    }
    if (rij.contains("sample_rate")) {
        auto sample_rate = positive_double_from_json(rij["sample_rate"], "render_instructions.sample_rate");
        if (!sample_rate.has_value()) return std::unexpected(std::move(sample_rate).error());
        ri.sample_rate = *sample_rate;
    }
    if (rij.contains("resample_ratio")) {
        auto resample_ratio = positive_double_from_json(rij["resample_ratio"],
                                                       "render_instructions.resample_ratio");
        if (!resample_ratio.has_value()) return std::unexpected(std::move(resample_ratio).error());
        ri.resample_ratio = *resample_ratio;
    }
    if (rij.contains("impairments")) {
        auto imp = impairment_settings_from_json(rij["impairments"]);
        if (!imp.has_value()) return std::unexpected(std::move(imp).error());
        ri.impairments = *imp;
    }
    return ri;
}

nlohmann::json resource_estimate_to_json(const ResourceEstimate& estimate) {
    return nlohmann::json{
        {"estimated_cpu_load", estimate.estimated_cpu_load},
        {"peak_memory_bytes", estimate.peak_memory_bytes},
        {"min_inter_emitter_gap_sec", estimate.min_inter_emitter_gap_sec},
        {"timing_feasible", estimate.timing_feasible},
        {"warnings", estimate.warnings},
    };
}

std::expected<ResourceEstimate, common::ErrorList> resource_estimate_from_json(const nlohmann::json& j) {
    auto obj = object_from_json(j, "resource_estimate");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"estimated_cpu_load", "peak_memory_bytes", "min_inter_emitter_gap_sec",
                            "timing_feasible", "warnings"}) {
        auto required = require_key(j, key, "resource_estimate");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    ResourceEstimate estimate;
    if (j.contains("estimated_cpu_load")) {
        auto estimated_cpu_load = non_negative_double_from_json(j["estimated_cpu_load"],
                                                               "resource_estimate.estimated_cpu_load");
        if (!estimated_cpu_load.has_value()) return std::unexpected(std::move(estimated_cpu_load).error());
        estimate.estimated_cpu_load = *estimated_cpu_load;
    }
    if (j.contains("peak_memory_bytes")) {
        auto peak_memory_bytes = size_from_json(j["peak_memory_bytes"], "resource_estimate.peak_memory_bytes");
        if (!peak_memory_bytes.has_value()) return std::unexpected(std::move(peak_memory_bytes).error());
        estimate.peak_memory_bytes = *peak_memory_bytes;
    }
    if (j.contains("min_inter_emitter_gap_sec")) {
        auto gap = non_negative_double_from_json(j["min_inter_emitter_gap_sec"],
                                                "resource_estimate.min_inter_emitter_gap_sec");
        if (!gap.has_value()) return std::unexpected(std::move(gap).error());
        estimate.min_inter_emitter_gap_sec = *gap;
    }
    if (j.contains("timing_feasible")) {
        auto timing_feasible = bool_from_json(j["timing_feasible"], "resource_estimate.timing_feasible");
        if (!timing_feasible.has_value()) return std::unexpected(std::move(timing_feasible).error());
        estimate.timing_feasible = *timing_feasible;
    }
    if (j.contains("warnings")) {
        auto warnings = string_array_from_json(j["warnings"], "resource_estimate.warnings");
        if (!warnings.has_value()) return std::unexpected(std::move(warnings).error());
        estimate.warnings = std::move(*warnings);
    }
    return estimate;
}

std::expected<void, common::ErrorList> validate_plan_identity(const Plan& plan) {
    std::unordered_set<std::string> render_ids;
    for (const auto& ri : plan.render_instructions) {
        auto waveform_params = validate_waveform_params(
            ri.waveform, "render_instructions waveform for emitter_id '" + ri.emitter_id + "'");
        if (!waveform_params.has_value()) return std::unexpected(std::move(waveform_params).error());
        if (ri.emitter_id.empty()) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_EMPTY_RENDER_ID",
                "render_instructions emitter_id must be non-empty"));
        }
        if (!render_ids.insert(ri.emitter_id).second) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_DUPLICATE_RENDER_ID",
                "Duplicate render instruction emitter_id in render_instructions: '" + ri.emitter_id + "'"));
        }
    }

    const auto expected_channels = expected_channel_ids_by_binding(plan.normalized_scenario);
    std::unordered_set<std::string> channel_bindings;
    std::unordered_set<uint32_t> channel_indices;
    std::unordered_map<uint32_t, std::string> channel_ids_by_index;
    std::unordered_set<std::string> device_ids;
    std::unordered_map<uint32_t, std::unordered_set<std::string>> devices_by_channel_index;
    for (const auto& ch : plan.channels) {
        if (ch.device_id.empty()) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_EMPTY_CHANNEL_DEVICE_ID",
                "channels.device_id must be non-empty"));
        }
        const auto binding = channel_binding_key(ch.device_id, ch.channel_index);
        if (!channel_bindings.insert(binding).second) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_DUPLICATE_CHANNEL_BINDING",
                "Duplicate channel binding in channels: '" + binding + "'"));
        }
        if (!channel_indices.insert(ch.channel_index).second) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_DUPLICATE_CHANNEL_INDEX",
                "Duplicate channel_index in channels: " + std::to_string(ch.channel_index)));
        }
        auto expected_channel = expected_channels.find(binding);
        if (expected_channel == expected_channels.end()) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_DANGLING_CHANNEL_BINDING",
                "channels references device/channel not present in normalized_scenario: '" +
                    binding + "'"));
        }
        channel_ids_by_index[ch.channel_index] = expected_channel->second;
        device_ids.insert(ch.device_id);
        devices_by_channel_index[ch.channel_index].insert(ch.device_id);
    }

    for (const auto& event : plan.timeline) {
        auto payload_shape = validate_timeline_payload_shape(event);
        if (!payload_shape.has_value()) return std::unexpected(std::move(payload_shape).error());

        if (event.type == TimelineEventType::EmitterStart ||
            event.type == TimelineEventType::EmitterStop) {
            if (!render_ids.count(event.target_id)) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_DANGLING_TIMELINE_TARGET",
                    "timeline target_id references emitter_id not present in render_instructions: '" +
                        event.target_id + "'"));
            }
            continue;
        }

        if (event.type == TimelineEventType::WaveformSwitch ||
            event.type == TimelineEventType::ImpairmentChange) {
            const auto emitter_id = event.payload["emitter_id"].get<std::string>();
            if (!render_ids.count(emitter_id)) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_DANGLING_TIMELINE_PAYLOAD_EMITTER",
                    "timeline payload.emitter_id references emitter_id not present in "
                    "render_instructions: '" + emitter_id + "'"));
            }
        }

        if (!device_ids.count(event.target_id)) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_DANGLING_TIMELINE_TARGET",
                "timeline target_id references device_id not present in channels: '" +
                    event.target_id + "'"));
        }

        if (event.payload.contains("channel")) {
            auto channel = uint32_from_json(event.payload["channel"], "timeline.payload.channel");
            if (!channel.has_value()) return std::unexpected(std::move(channel).error());
            if (!channel_bindings.count(event.target_id + ":" + std::to_string(*channel))) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_DANGLING_TIMELINE_CHANNEL",
                    "timeline payload.channel references device/channel not present in channels: '" +
                        event.target_id + ":" + std::to_string(*channel) + "'"));
            }
        }
    }

    std::unordered_set<uint32_t> channel_plan_indices;
    std::unordered_set<std::string> channel_plan_render_ids;
    for (const auto& cp : plan.channel_plans) {
        if (cp.channel_id.empty()) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_EMPTY_CHANNEL_PLAN_ID",
                "channel_plans.channel_id must be non-empty"));
        }
        if (!channel_plan_indices.insert(cp.channel_index).second) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_DUPLICATE_CHANNEL_PLAN",
                "Duplicate channel_plans.channel_index: " + std::to_string(cp.channel_index)));
        }
        if (!channel_indices.count(cp.channel_index)) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_DANGLING_CHANNEL_PLAN",
                "Channel plan references channel_index not present in channels: " +
                    std::to_string(cp.channel_index)));
        }
        auto channel_id = channel_ids_by_index.find(cp.channel_index);
        if (channel_id != channel_ids_by_index.end() && cp.channel_id != channel_id->second) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_MISMATCHED_CHANNEL_PLAN_ID",
                "channel_plans.channel_id does not match normalized_scenario channel for "
                "channel_index " + std::to_string(cp.channel_index) + ": expected '" +
                    channel_id->second + "', got '" + cp.channel_id + "'"));
        }

        std::unordered_set<std::string> channel_render_ids;
        for (const auto& ri : cp.render_instructions) {
            auto waveform_params = validate_waveform_params(
                ri.waveform,
                "channel_plans.render_instructions waveform for emitter_id '" + ri.emitter_id + "'");
            if (!waveform_params.has_value()) return std::unexpected(std::move(waveform_params).error());
            if (!channel_render_ids.insert(ri.emitter_id).second) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_DUPLICATE_RENDER_ID",
                    "Duplicate render instruction emitter_id in channel_plans.render_instructions: '" +
                        ri.emitter_id + "'"));
            }
            if (!channel_plan_render_ids.insert(ri.emitter_id).second) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_DUPLICATE_CHANNEL_PLAN_RENDER_ID",
                    "render instruction emitter_id appears in multiple channel_plans: '" +
                        ri.emitter_id + "'"));
            }
            if (!render_ids.count(ri.emitter_id)) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_DANGLING_RENDER_ID",
                    "channel_plans.render_instructions references emitter_id not present in render_instructions: '" +
                        ri.emitter_id + "'"));
            }
        }

        auto channel_devices = devices_by_channel_index.find(cp.channel_index);
        for (const auto& event : cp.events) {
            auto payload_shape = validate_timeline_payload_shape(event);
            if (!payload_shape.has_value()) return std::unexpected(std::move(payload_shape).error());

            if (event.type == TimelineEventType::EmitterStart ||
                event.type == TimelineEventType::EmitterStop) {
                if (!channel_render_ids.count(event.target_id)) {
                    return std::unexpected(plan_io_error(
                        "E_PLAN_IO_DANGLING_CHANNEL_PLAN_EVENT_TARGET",
                        "channel_plans.events target_id references emitter_id not present in "
                        "channel_plans.render_instructions: '" + event.target_id + "'"));
                }
                continue;
            }

            if (event.type == TimelineEventType::WaveformSwitch ||
                event.type == TimelineEventType::ImpairmentChange) {
                const auto emitter_id = event.payload["emitter_id"].get<std::string>();
                if (!channel_render_ids.count(emitter_id)) {
                    return std::unexpected(plan_io_error(
                        "E_PLAN_IO_DANGLING_CHANNEL_PLAN_EVENT_PAYLOAD_EMITTER",
                        "channel_plans.events payload.emitter_id references emitter_id not present in "
                        "channel_plans.render_instructions: '" + emitter_id + "'"));
                }
            }

            if (channel_devices == devices_by_channel_index.end() ||
                !channel_devices->second.count(event.target_id)) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_DANGLING_CHANNEL_PLAN_EVENT_TARGET",
                    "channel_plans.events target_id references device_id not bound to "
                    "channel_plans.channel_index " + std::to_string(cp.channel_index) + ": '" +
                        event.target_id + "'"));
            }

            if (event.payload.contains("channel")) {
                auto channel = uint32_from_json(event.payload["channel"],
                                                "channel_plans.events.payload.channel");
                if (!channel.has_value()) return std::unexpected(std::move(channel).error());
                if (*channel != cp.channel_index ||
                    !channel_bindings.count(event.target_id + ":" + std::to_string(*channel))) {
                    return std::unexpected(plan_io_error(
                        "E_PLAN_IO_DANGLING_CHANNEL_PLAN_EVENT_CHANNEL",
                        "channel_plans.events payload.channel references device/channel not matching "
                        "channel_plans.channel_index: '" + event.target_id + ":" +
                            std::to_string(*channel) + "'"));
                }
            }
        }
    }
    if (!plan.channel_plans.empty()) {
        for (const auto& emitter_id : render_ids) {
            if (!channel_plan_render_ids.count(emitter_id)) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_MISSING_CHANNEL_PLAN_RENDER_ID",
                    "render instruction emitter_id is missing from channel_plans.render_instructions: '" +
                        emitter_id + "'"));
            }
        }
    }

    for (const auto& mg : plan.mix_groups) {
        if (mg.device_id.empty()) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_EMPTY_MIX_DEVICE_ID",
                "mix_groups.device_id must be non-empty"));
        }
        if (!channel_bindings.count(mg.device_id + ":" + std::to_string(mg.channel))) {
            return std::unexpected(plan_io_error(
                "E_PLAN_IO_DANGLING_MIX_CHANNEL",
                "Mix group references device/channel not present in channels: '" + mg.device_id +
                    ":" + std::to_string(mg.channel) + "'"));
        }

        std::unordered_set<std::string> member_ids;
        for (const auto& emitter_id : mg.emitter_ids) {
            if (emitter_id.empty()) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_EMPTY_MIX_MEMBER",
                    "mix_groups.emitter_ids entries must be non-empty"));
            }
            if (!member_ids.insert(emitter_id).second) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_DUPLICATE_MIX_MEMBER",
                    "Duplicate emitter_id in mix_groups.emitter_ids: '" + emitter_id + "'"));
            }
            if (!render_ids.count(emitter_id)) {
                return std::unexpected(plan_io_error(
                    "E_PLAN_IO_DANGLING_MIX_MEMBER",
                    "mix_groups.emitter_ids references emitter_id not present in render_instructions: '" +
                        emitter_id + "'"));
            }
        }
    }

    auto schedule_windows = validate_plan_schedule_windows(plan);
    if (!schedule_windows.has_value()) {
        return std::unexpected(std::move(schedule_windows).error());
    }

    return {};
}

nlohmann::json error_to_json(const common::Error& err) {
    return nlohmann::json{
        {"category", common::category_to_string(err.category)},
        {"code", err.code},
        {"message", err.message}};
}

common::Error error_from_json(const nlohmann::json& j) {
    auto parsed = diagnostic_from_json(j, "error");
    if (!parsed.has_value()) {
        const auto& errors = parsed.error();
        throw std::invalid_argument(errors.empty() ? "invalid diagnostic JSON" : errors.front().message);
    }
    return *parsed;
}

nlohmann::json plan_to_json(const Plan& plan) {
    nlohmann::json j;

    j["normalized_scenario"] = scenario_to_json(plan.normalized_scenario);

    nlohmann::json channels_arr = nlohmann::json::array();
    for (const auto& ch : plan.channels) {
        nlohmann::json chj;
        chj["device_id"] = ch.device_id;
        chj["channel_index"] = ch.channel_index;
        chj["rf"] = rf_settings_to_json(ch.rf);
        channels_arr.push_back(chj);
    }
    j["channels"] = channels_arr;

    nlohmann::json timeline_arr = nlohmann::json::array();
    for (const auto& ev : plan.timeline) {
        timeline_arr.push_back(timeline_event_to_json(ev));
    }
    j["timeline"] = timeline_arr;

    nlohmann::json render_arr = nlohmann::json::array();
    for (const auto& ri : plan.render_instructions) {
        render_arr.push_back(render_instruction_to_json(ri));
    }
    j["render_instructions"] = render_arr;

    nlohmann::json warnings_arr = nlohmann::json::array();
    for (const auto& w : plan.warnings) {
        warnings_arr.push_back(error_to_json(w));
    }
    j["warnings"] = warnings_arr;

    j["estimated_duration_sec"] = plan.estimated_duration_sec;
    j["resource_estimate"] = resource_estimate_to_json(plan.resource_estimate);

    nlohmann::json mix_groups_arr = nlohmann::json::array();
    for (const auto& mg : plan.mix_groups) {
        nlohmann::json mgj;
        mgj["device_id"] = mg.device_id;
        mgj["channel"] = mg.channel;
        mgj["start_sec"] = mg.start_sec;
        mgj["duration_sec"] = mg.duration_sec;
        mgj["emitter_ids"] = mg.emitter_ids;
        mgj["estimated_peak_sum"] = mg.estimated_peak_sum;
        mix_groups_arr.push_back(mgj);
    }
    j["mix_groups"] = mix_groups_arr;

    nlohmann::json channel_plans_arr = nlohmann::json::array();
    for (const auto& cp : plan.channel_plans) {
        nlohmann::json cpj;
        cpj["channel_id"] = cp.channel_id;
        cpj["channel_index"] = cp.channel_index;
        cpj["rf"] = rf_settings_to_json(cp.rf);
        nlohmann::json cp_render = nlohmann::json::array();
        for (const auto& ri : cp.render_instructions) {
            cp_render.push_back(render_instruction_to_json(ri));
        }
        cpj["render_instructions"] = cp_render;
        nlohmann::json cp_events = nlohmann::json::array();
        for (const auto& event : cp.events) {
            cp_events.push_back(timeline_event_to_json(event));
        }
        cpj["events"] = cp_events;
        cpj["resource_estimate"] = resource_estimate_to_json(cp.resource_estimate);
        channel_plans_arr.push_back(cpj);
    }
    j["channel_plans"] = channel_plans_arr;

    j["run_mode"] = (plan.run_mode == RunMode::Replay) ? "replay" : "realtime";

    return j;
}

std::expected<Plan, common::ErrorList> plan_from_json(const nlohmann::json& j) {
    try {
    auto obj = object_from_json(j, "plan");
    if (!obj.has_value()) return std::unexpected(std::move(obj).error());

    for (const char* key : {"normalized_scenario", "channels", "timeline", "render_instructions",
                            "warnings", "estimated_duration_sec", "resource_estimate", "mix_groups",
                            "channel_plans", "run_mode"}) {
        auto required = require_key(j, key, "plan");
        if (!required.has_value()) return std::unexpected(std::move(required).error());
    }

    Plan plan;

    if (j.contains("normalized_scenario")) {
        auto s = scenario_from_json(j["normalized_scenario"]);
        if (!s.has_value()) return std::unexpected(std::move(s).error());
        auto validation = validate(*s);
        if (!validation.ok()) return std::unexpected(std::move(validation.errors));
        plan.normalized_scenario = *s;
    }

    if (j.contains("channels")) {
        auto channels = array_from_json(j["channels"], "channels");
        if (!channels.has_value()) return std::unexpected(std::move(channels).error());
        for (const auto& chj : **channels) {
            auto channel_obj = object_from_json(chj, "channels");
            if (!channel_obj.has_value()) return std::unexpected(std::move(channel_obj).error());
            for (const char* key : {"device_id", "channel_index", "rf"}) {
                auto required = require_key(chj, key, "channels");
                if (!required.has_value()) return std::unexpected(std::move(required).error());
            }
            ChannelBinding ch;
            if (chj.contains("device_id")) {
                auto device_id = string_from_json(chj["device_id"], "channels.device_id");
                if (!device_id.has_value()) return std::unexpected(std::move(device_id).error());
                ch.device_id = *device_id;
            }
            if (chj.contains("channel_index")) {
                auto channel_index = uint32_from_json(chj["channel_index"], "channels.channel_index");
                if (!channel_index.has_value()) return std::unexpected(std::move(channel_index).error());
                ch.channel_index = *channel_index;
            }
            auto required_rf = require_key(chj, "rf", "channels");
            if (!required_rf.has_value()) return std::unexpected(std::move(required_rf).error());
            auto rf = rf_settings_from_json(chj["rf"]);
            if (!rf.has_value()) return std::unexpected(std::move(rf).error());
            ch.rf = *rf;
            plan.channels.push_back(std::move(ch));
        }
    }

    if (j.contains("timeline")) {
        auto timeline = array_from_json(j["timeline"], "timeline");
        if (!timeline.has_value()) return std::unexpected(std::move(timeline).error());
        for (const auto& evj : **timeline) {
            auto ev = timeline_event_from_json(evj);
            if (!ev.has_value()) return std::unexpected(std::move(ev).error());
            plan.timeline.push_back(std::move(*ev));
        }
    }

    if (j.contains("render_instructions")) {
        auto render_instructions = array_from_json(j["render_instructions"], "render_instructions");
        if (!render_instructions.has_value()) return std::unexpected(std::move(render_instructions).error());
        for (const auto& rij : **render_instructions) {
            auto ri = render_instruction_from_json(rij);
            if (!ri.has_value()) return std::unexpected(std::move(ri).error());
            plan.render_instructions.push_back(std::move(*ri));
        }
    }

    if (j.contains("warnings")) {
        auto warnings = array_from_json(j["warnings"], "warnings");
        if (!warnings.has_value()) return std::unexpected(std::move(warnings).error());
        for (const auto& wj : **warnings) {
            auto warning = diagnostic_from_json(wj, "warnings[]");
            if (!warning.has_value()) return std::unexpected(std::move(warning).error());
            plan.warnings.push_back(std::move(*warning));
        }
    }

    if (j.contains("estimated_duration_sec")) {
        auto estimated_duration_sec = non_negative_double_from_json(j["estimated_duration_sec"],
                                                                   "estimated_duration_sec");
        if (!estimated_duration_sec.has_value()) return std::unexpected(std::move(estimated_duration_sec).error());
        plan.estimated_duration_sec = *estimated_duration_sec;
    }
    if (j.contains("resource_estimate")) {
        auto estimate = resource_estimate_from_json(j["resource_estimate"]);
        if (!estimate.has_value()) return std::unexpected(std::move(estimate).error());
        plan.resource_estimate = *estimate;
    }

    if (j.contains("mix_groups")) {
        auto mix_groups = array_from_json(j["mix_groups"], "mix_groups");
        if (!mix_groups.has_value()) return std::unexpected(std::move(mix_groups).error());
        for (const auto& mgj : **mix_groups) {
            auto mix_group_obj = object_from_json(mgj, "mix_groups");
            if (!mix_group_obj.has_value()) return std::unexpected(std::move(mix_group_obj).error());
            for (const char* key : {"device_id", "channel", "start_sec", "duration_sec",
                                    "emitter_ids", "estimated_peak_sum"}) {
                auto required = require_key(mgj, key, "mix_groups");
                if (!required.has_value()) return std::unexpected(std::move(required).error());
            }
            MixGroup mg;
            if (mgj.contains("device_id")) {
                auto device_id = string_from_json(mgj["device_id"], "mix_groups.device_id");
                if (!device_id.has_value()) return std::unexpected(std::move(device_id).error());
                mg.device_id = *device_id;
            }
            if (mgj.contains("channel")) {
                auto channel = uint32_from_json(mgj["channel"], "mix_groups.channel");
                if (!channel.has_value()) return std::unexpected(std::move(channel).error());
                mg.channel = *channel;
            }
            if (mgj.contains("start_sec")) {
                auto start_sec = non_negative_double_from_json(mgj["start_sec"], "mix_groups.start_sec");
                if (!start_sec.has_value()) return std::unexpected(std::move(start_sec).error());
                mg.start_sec = *start_sec;
            }
            if (mgj.contains("duration_sec")) {
                auto duration_sec = positive_double_from_json(mgj["duration_sec"], "mix_groups.duration_sec");
                if (!duration_sec.has_value()) return std::unexpected(std::move(duration_sec).error());
                mg.duration_sec = *duration_sec;
            }
            if (mgj.contains("estimated_peak_sum")) {
                auto peak_sum = non_negative_double_from_json(mgj["estimated_peak_sum"],
                                                             "mix_groups.estimated_peak_sum");
                if (!peak_sum.has_value()) return std::unexpected(std::move(peak_sum).error());
                mg.estimated_peak_sum = *peak_sum;
            }
            if (mgj.contains("emitter_ids")) {
                auto emitter_ids = string_array_from_json(mgj["emitter_ids"], "mix_groups.emitter_ids");
                if (!emitter_ids.has_value()) return std::unexpected(std::move(emitter_ids).error());
                mg.emitter_ids = std::move(*emitter_ids);
            }
            plan.mix_groups.push_back(std::move(mg));
        }
    }

    if (j.contains("channel_plans")) {
        auto channel_plans = array_from_json(j["channel_plans"], "channel_plans");
        if (!channel_plans.has_value()) return std::unexpected(std::move(channel_plans).error());
        for (const auto& cpj : **channel_plans) {
            auto channel_plan_obj = object_from_json(cpj, "channel_plans");
            if (!channel_plan_obj.has_value()) return std::unexpected(std::move(channel_plan_obj).error());
            for (const char* key : {"channel_id", "channel_index", "rf", "render_instructions",
                                    "events", "resource_estimate"}) {
                auto required = require_key(cpj, key, "channel_plans");
                if (!required.has_value()) return std::unexpected(std::move(required).error());
            }
            ChannelPlan cp;
            if (cpj.contains("channel_id")) {
                auto channel_id = string_from_json(cpj["channel_id"], "channel_plans.channel_id");
                if (!channel_id.has_value()) return std::unexpected(std::move(channel_id).error());
                cp.channel_id = *channel_id;
            }
            if (cpj.contains("channel_index")) {
                auto channel_index = uint32_from_json(cpj["channel_index"], "channel_plans.channel_index");
                if (!channel_index.has_value()) return std::unexpected(std::move(channel_index).error());
                cp.channel_index = *channel_index;
            }
            auto required_rf = require_key(cpj, "rf", "channel_plans");
            if (!required_rf.has_value()) return std::unexpected(std::move(required_rf).error());
            auto rf = rf_settings_from_json(cpj["rf"]);
            if (!rf.has_value()) return std::unexpected(std::move(rf).error());
            cp.rf = *rf;
            if (cpj.contains("render_instructions")) {
                auto render_instructions = array_from_json(cpj["render_instructions"],
                                                           "channel_plans.render_instructions");
                if (!render_instructions.has_value()) {
                    return std::unexpected(std::move(render_instructions).error());
                }
                for (const auto& rij : **render_instructions) {
                    auto ri = render_instruction_from_json(rij);
                    if (!ri.has_value()) return std::unexpected(std::move(ri).error());
                    cp.render_instructions.push_back(std::move(*ri));
                }
            }
            if (cpj.contains("events")) {
                auto events = array_from_json(cpj["events"], "channel_plans.events");
                if (!events.has_value()) return std::unexpected(std::move(events).error());
                for (const auto& evj : **events) {
                    auto ev = timeline_event_from_json(evj);
                    if (!ev.has_value()) return std::unexpected(std::move(ev).error());
                    cp.events.push_back(std::move(*ev));
                }
            }
            if (cpj.contains("resource_estimate")) {
                auto estimate = resource_estimate_from_json(cpj["resource_estimate"]);
                if (!estimate.has_value()) return std::unexpected(std::move(estimate).error());
                cp.resource_estimate = *estimate;
            }
            plan.channel_plans.push_back(std::move(cp));
        }
    }

    if (j.contains("run_mode")) {
        auto mode = string_from_json(j["run_mode"], "run_mode");
        if (!mode.has_value()) return std::unexpected(std::move(mode).error());
        auto run_mode = run_mode_from_string(*mode);
        if (!run_mode.has_value()) return std::unexpected(std::move(run_mode).error());
        plan.run_mode = *run_mode;
    }

    auto identity = validate_plan_identity(plan);
    if (!identity.has_value()) return std::unexpected(std::move(identity).error());

    return plan;
    } catch (const nlohmann::json::exception& e) {
        return std::unexpected(plan_io_error("E_PLAN_IO_BAD_JSON",
                                             std::string("Invalid plan JSON shape: ") + e.what()));
    }
}

} // namespace archerfish::scenario
