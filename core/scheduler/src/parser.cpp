#include "archerfish/scenario/parser.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#ifdef ARCHERFISH_WITH_YAML
#include <yaml-cpp/yaml.h>
#endif

#include "archerfish/dsp/waveform_type.hpp"

namespace archerfish::scenario {

using json = nlohmann::json;
using namespace archerfish::common;

#ifdef ARCHERFISH_WITH_YAML
static nlohmann::json yaml_node_to_json(const YAML::Node& node) {
    switch (node.Type()) {
        case YAML::NodeType::Null:
            return nullptr;
        case YAML::NodeType::Scalar: {
            try { return node.as<int>(); } catch (...) {}
            try { return node.as<double>(); } catch (...) {}
            return node.as<std::string>();
        }
        case YAML::NodeType::Sequence: {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& child : node) {
                arr.push_back(yaml_node_to_json(child));
            }
            return arr;
        }
        case YAML::NodeType::Map: {
            nlohmann::json obj = nlohmann::json::object();
            for (const auto& kv : node) {
                obj[kv.first.as<std::string>()] = yaml_node_to_json(kv.second);
            }
            return obj;
        }
        default:
            return nullptr;
    }
}
#endif

static bool is_yaml_extension(const std::filesystem::path& p) {
    std::string ext = p.extension().string();
    return ext == ".yaml" || ext == ".yml";
}

static Error make_error(ErrorCategory cat, std::string code, std::string message) {
    return Error{cat, std::move(code), std::move(message)};
}

static Error make_type_error(std::string field, std::string expected) {
    return make_error(ErrorCategory::Config, "E_JSON_TYPE",
                      fmt::format("'{}' must be {}", field, expected));
}

static std::expected<uint32_t, Error> parse_uint32_value(const json& value, std::string field) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return std::unexpected(make_type_error(std::move(field), "an unsigned 32-bit integer"));
    }

    if (value.is_number_integer()) {
        const auto parsed = value.get<std::int64_t>();
        if (parsed < 0) {
            return std::unexpected(make_error(ErrorCategory::Config, "E_JSON_RANGE",
                                              fmt::format("'{}' must be non-negative", field)));
        }
        if (parsed > static_cast<std::int64_t>(std::numeric_limits<uint32_t>::max())) {
            return std::unexpected(make_error(ErrorCategory::Config, "E_JSON_RANGE",
                                              fmt::format("'{}' exceeds uint32 range", field)));
        }
        return static_cast<uint32_t>(parsed);
    }

    const auto parsed = value.get<std::uint64_t>();
    if (parsed > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(make_error(ErrorCategory::Config, "E_JSON_RANGE",
                                          fmt::format("'{}' exceeds uint32 range", field)));
    }
    return static_cast<uint32_t>(parsed);
}

static std::expected<int, Error> parse_non_negative_int_value(const json& value, std::string field) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return std::unexpected(make_type_error(std::move(field), "a non-negative integer"));
    }

    if (value.is_number_integer()) {
        const auto parsed = value.get<std::int64_t>();
        if (parsed < 0) {
            return std::unexpected(make_error(ErrorCategory::Config, "E_JSON_RANGE",
                                              fmt::format("'{}' must be non-negative", field)));
        }
        if (parsed > std::numeric_limits<int>::max()) {
            return std::unexpected(make_error(ErrorCategory::Config, "E_JSON_RANGE",
                                              fmt::format("'{}' exceeds int range", field)));
        }
        return static_cast<int>(parsed);
    }

    const auto parsed = value.get<std::uint64_t>();
    if (parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return std::unexpected(make_error(ErrorCategory::Config, "E_JSON_RANGE",
                                          fmt::format("'{}' exceeds int range", field)));
    }
    return static_cast<int>(parsed);
}

static std::expected<std::string, Error> parse_string_value(const json& value, std::string field) {
    if (!value.is_string()) {
        return std::unexpected(make_type_error(std::move(field), "a string"));
    }
    return value.get<std::string>();
}

static std::expected<double, Error> parse_double_value(const json& value, std::string field) {
    if (!value.is_number()) {
        return std::unexpected(make_type_error(std::move(field), "a finite number"));
    }
    const double parsed = value.get<double>();
    if (!std::isfinite(parsed)) {
        return std::unexpected(make_error(ErrorCategory::Config, "E_JSON_RANGE",
                                          fmt::format("'{}' must be finite", field)));
    }
    return parsed;
}

static void assign_double_value(const json& value, std::string field, double& out, ErrorList& errors) {
    auto parsed = parse_double_value(value, std::move(field));
    if (parsed.has_value()) {
        out = *parsed;
    } else {
        errors.push_back(std::move(parsed.error()));
    }
}

static void assign_optional_double_value(const json& value,
                                         std::string field,
                                         std::optional<double>& out,
                                         ErrorList& errors) {
    auto parsed = parse_double_value(value, std::move(field));
    if (parsed.has_value()) {
        out = *parsed;
    } else {
        errors.push_back(std::move(parsed.error()));
    }
}

static void assign_string_value(const json& value, std::string field, std::string& out, ErrorList& errors) {
    auto parsed = parse_string_value(value, std::move(field));
    if (parsed.has_value()) {
        out = std::move(*parsed);
    } else {
        errors.push_back(std::move(parsed.error()));
    }
}

static void assign_optional_string_value(const json& value,
                                         std::string field,
                                         std::optional<std::string>& out,
                                         ErrorList& errors) {
    auto parsed = parse_string_value(value, std::move(field));
    if (parsed.has_value()) {
        out = std::move(*parsed);
    } else {
        errors.push_back(std::move(parsed.error()));
    }
}

static std::expected<bool, Error> parse_bool_value(const json& value, std::string field) {
    if (!value.is_boolean()) {
        return std::unexpected(make_type_error(std::move(field), "a boolean"));
    }
    return value.get<bool>();
}

static void assign_bool_value(const json& value, std::string field, bool& out, ErrorList& errors) {
    auto parsed = parse_bool_value(value, std::move(field));
    if (parsed.has_value()) {
        out = *parsed;
    } else {
        errors.push_back(std::move(parsed.error()));
    }
}

static std::expected<RfSettings, ErrorList> parse_rf_settings(const json& j) {
    ErrorList errors;
    RfSettings rf;

    if (!j.is_object()) {
        return std::unexpected(ErrorList{make_type_error("rf", "an object")});
    }

    if (!j.contains("freq_hz")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "rf.freq_hz is required"));
    } else {
        assign_double_value(j.at("freq_hz"), "rf.freq_hz", rf.freq_hz, errors);
    }

    if (!j.contains("rate_sps")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "rf.rate_sps is required"));
    } else {
        assign_double_value(j.at("rate_sps"), "rf.rate_sps", rf.rate_sps, errors);
    }

    if (!j.contains("gain_db")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "rf.gain_db is required"));
    } else {
        assign_double_value(j.at("gain_db"), "rf.gain_db", rf.gain_db, errors);
    }

    if (j.contains("bandwidth_hz"))
        assign_optional_double_value(j.at("bandwidth_hz"), "rf.bandwidth_hz", rf.bandwidth_hz, errors);
    if (j.contains("antenna")) assign_optional_string_value(j.at("antenna"), "rf.antenna", rf.antenna, errors);

    if (!errors.empty()) return std::unexpected(std::move(errors));
    return rf;
}

static std::expected<DeviceDef, ErrorList> parse_device(const json& j) {
    ErrorList errors;
    DeviceDef dev;

    if (!j.is_object()) {
        return std::unexpected(ErrorList{make_type_error("device", "an object")});
    }

    if (!j.contains("id")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "device id is required"));
    } else {
        assign_string_value(j.at("id"), "device.id", dev.id, errors);
    }

    if (j.contains("channel")) {
        auto channel = parse_uint32_value(j.at("channel"), "device.channel");
        if (channel.has_value()) {
            dev.channel = *channel;
        } else {
            errors.push_back(std::move(channel.error()));
        }
    }

    if (!j.contains("rf")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD",
                                     fmt::format("device '{}' missing rf section", dev.id)));
    } else {
        auto rf_result = parse_rf_settings(j.at("rf"));
        if (rf_result.has_value()) {
            dev.rf = std::move(*rf_result);
        } else {
            for (auto& e : rf_result.error()) errors.push_back(std::move(e));
        }
    }

    if (!errors.empty()) return std::unexpected(std::move(errors));
    return dev;
}

static std::expected<WaveformDef, ErrorList> parse_waveform(const json& j, std::optional<std::string> id = std::nullopt) {
    if (!j.is_object()) {
        return std::unexpected(ErrorList{make_type_error("waveform", "an object")});
    }

    WaveformDef wf;
    wf.id = std::move(id);
    if (!wf.id.has_value() && j.contains("id")) {
        auto parsed_id = parse_string_value(j.at("id"), "waveform.id");
        if (!parsed_id.has_value()) return std::unexpected(ErrorList{std::move(parsed_id.error())});
        wf.id = std::move(*parsed_id);
    }

    if (!j.contains("type")) {
        return std::unexpected(ErrorList{
            make_error(ErrorCategory::Config, "E_MISSING_FIELD", "waveform missing required 'type' field")});
    }
    auto parsed_type = parse_string_value(j.at("type"), "waveform.type");
    if (!parsed_type.has_value()) return std::unexpected(ErrorList{std::move(parsed_type.error())});
    auto type_str = *parsed_type;
    auto type_result = dsp::waveform_type_from_string(type_str);
    if (!type_result.has_value()) {
        return std::unexpected(ErrorList{
            make_error(ErrorCategory::Config, "E_INVALID_WAVEFORM_TYPE", type_result.error())});
    }
    wf.type = *type_result;

    ErrorList errors;
    if (j.contains("target_power_dbm")) {
        assign_optional_double_value(j.at("target_power_dbm"), "waveform.target_power_dbm",
                                     wf.target_power_dbm, errors);
    }
    if (!errors.empty()) return std::unexpected(std::move(errors));

    json params = j;
    params.erase("type");
    params.erase("target_power_dbm");
    if (wf.id.has_value()) params.erase("id");
    wf.params = params;

    return wf;
}

static std::expected<ImpairmentSettings, ErrorList> parse_impairments(const json& j) {
    if (!j.is_object()) {
        return std::unexpected(ErrorList{make_type_error("impairments", "an object")});
    }

    ImpairmentSettings imp;
    ErrorList errors;
    if (j.contains("cfo_hz")) assign_optional_double_value(j.at("cfo_hz"), "impairments.cfo_hz", imp.cfo_hz, errors);
    if (j.contains("phase_offset_rad"))
        assign_optional_double_value(j.at("phase_offset_rad"), "impairments.phase_offset_rad",
                                     imp.phase_offset_rad, errors);
    if (j.contains("iq_gain_imbalance_db"))
        assign_optional_double_value(j.at("iq_gain_imbalance_db"), "impairments.iq_gain_imbalance_db",
                                     imp.iq_gain_imbalance_db, errors);
    if (j.contains("iq_phase_imbalance_rad"))
        assign_optional_double_value(j.at("iq_phase_imbalance_rad"), "impairments.iq_phase_imbalance_rad",
                                     imp.iq_phase_imbalance_rad, errors);
    if (j.contains("dc_offset_i"))
        assign_optional_double_value(j.at("dc_offset_i"), "impairments.dc_offset_i", imp.dc_offset_i, errors);
    if (j.contains("dc_offset_q"))
        assign_optional_double_value(j.at("dc_offset_q"), "impairments.dc_offset_q", imp.dc_offset_q, errors);
    if (j.contains("awgn_power"))
        assign_optional_double_value(j.at("awgn_power"), "impairments.awgn_power", imp.awgn_power, errors);
    if (j.contains("amplitude_ripple_db"))
        assign_optional_double_value(j.at("amplitude_ripple_db"), "impairments.amplitude_ripple_db",
                                     imp.amplitude_ripple_db, errors);
    if (j.contains("amplitude_ripple_freq_hz"))
        assign_optional_double_value(j.at("amplitude_ripple_freq_hz"), "impairments.amplitude_ripple_freq_hz",
                                     imp.amplitude_ripple_freq_hz, errors);
    if (j.contains("delay_sec"))
        assign_optional_double_value(j.at("delay_sec"), "impairments.delay_sec", imp.delay_sec, errors);
    if (j.contains("burst_dropout_rate"))
        assign_optional_double_value(j.at("burst_dropout_rate"), "impairments.burst_dropout_rate",
                                     imp.burst_dropout_rate, errors);
    if (j.contains("burst_dropout_mean_burst_sec"))
        assign_optional_double_value(j.at("burst_dropout_mean_burst_sec"),
                                     "impairments.burst_dropout_mean_burst_sec",
                                     imp.burst_dropout_mean_burst_sec, errors);
    if (j.contains("phase_noise_bandwidth_hz"))
        assign_optional_double_value(j.at("phase_noise_bandwidth_hz"), "impairments.phase_noise_bandwidth_hz",
                                     imp.phase_noise_bandwidth_hz, errors);
    if (j.contains("phase_noise_magnitude_rad"))
        assign_optional_double_value(j.at("phase_noise_magnitude_rad"), "impairments.phase_noise_magnitude_rad",
                                     imp.phase_noise_magnitude_rad, errors);
    if (j.contains("phase_noise_psd_shape"))
        assign_optional_string_value(j.at("phase_noise_psd_shape"), "impairments.phase_noise_psd_shape",
                                     imp.phase_noise_psd_shape, errors);
    if (j.contains("multipath_delay_samples"))
        assign_optional_double_value(j.at("multipath_delay_samples"), "impairments.multipath_delay_samples",
                                     imp.multipath_delay_samples, errors);
    if (j.contains("multipath_amplitude"))
        assign_optional_double_value(j.at("multipath_amplitude"), "impairments.multipath_amplitude",
                                     imp.multipath_amplitude, errors);
    if (j.contains("fading_doppler_hz"))
        assign_optional_double_value(j.at("fading_doppler_hz"), "impairments.fading_doppler_hz",
                                     imp.fading_doppler_hz, errors);
    if (j.contains("fading_type"))
        assign_optional_string_value(j.at("fading_type"), "impairments.fading_type", imp.fading_type, errors);
    if (j.contains("fading_k_factor"))
        assign_optional_double_value(j.at("fading_k_factor"), "impairments.fading_k_factor",
                                     imp.fading_k_factor, errors);
    if (j.contains("pa_model"))
        assign_optional_string_value(j.at("pa_model"), "impairments.pa_model", imp.pa_model, errors);
    if (j.contains("pa_saturation"))
        assign_optional_double_value(j.at("pa_saturation"), "impairments.pa_saturation", imp.pa_saturation, errors);
    if (j.contains("pa_smoothness"))
        assign_optional_double_value(j.at("pa_smoothness"), "impairments.pa_smoothness", imp.pa_smoothness, errors);
    if (j.contains("pa_phase_shift"))
        assign_optional_double_value(j.at("pa_phase_shift"), "impairments.pa_phase_shift",
                                     imp.pa_phase_shift, errors);
    if (!errors.empty()) return std::unexpected(std::move(errors));
    return imp;
}

static std::expected<EmitterDef, ErrorList> parse_emitter(const json& j) {
    ErrorList errors;
    EmitterDef em;

    if (!j.is_object()) {
        return std::unexpected(ErrorList{make_type_error("emitter", "an object")});
    }

    if (!j.contains("id")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "emitter id is required"));
    } else {
        assign_string_value(j.at("id"), "emitter.id", em.id, errors);
    }

    if (!j.contains("device")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD",
                                     fmt::format("emitter '{}' missing device", em.id)));
    } else {
        assign_string_value(j.at("device"), "emitter.device", em.device, errors);
    }

    if (j.contains("channel")) {
        auto channel = parse_uint32_value(j.at("channel"), "emitter.channel");
        if (channel.has_value()) {
            em.channel = *channel;
        } else {
            errors.push_back(std::move(channel.error()));
        }
    }
    if (j.contains("start_after_sec"))
        assign_double_value(j.at("start_after_sec"), "emitter.start_after_sec", em.start_after_sec, errors);
    if (j.contains("duration_sec"))
        assign_double_value(j.at("duration_sec"), "emitter.duration_sec", em.duration_sec, errors);

    if (j.contains("waveform")) {
        if (!j.at("waveform").is_object()) {
            errors.push_back(make_type_error("emitter.waveform", "an object"));
        } else {
        auto wf_result = parse_waveform(j.at("waveform"));
        if (wf_result.has_value()) {
            em.waveform = std::move(*wf_result);
        } else {
            for (auto& e : wf_result.error()) errors.push_back(std::move(e));
        }
        }
    }
    if (j.contains("waveform_ref")) {
        assign_optional_string_value(j.at("waveform_ref"), "emitter.waveform_ref", em.waveform_ref, errors);
    }

    if (j.contains("impairments")) {
        if (!j.at("impairments").is_object()) {
            errors.push_back(make_type_error("emitter.impairments", "an object"));
        } else {
        auto imp_result = parse_impairments(j.at("impairments"));
        if (imp_result.has_value()) {
            em.impairments = std::move(*imp_result);
        } else {
            for (auto& e : imp_result.error()) errors.push_back(std::move(e));
        }
        }
    }

    if (j.contains("mixing")) {
        auto mixing = parse_string_value(j.at("mixing"), "emitter.mixing");
        if (mixing.has_value()) {
            if (*mixing == "additive") {
                em.mixing = MixingMode::Additive;
            } else if (*mixing == "none") {
                em.mixing = MixingMode::None;
            } else {
                errors.push_back(make_error(ErrorCategory::Config, "E_INVALID_MIXING_MODE",
                                            fmt::format("emitter '{}' has invalid mixing mode '{}'; expected 'none' or 'additive'",
                                                        em.id, *mixing)));
            }
        } else {
            errors.push_back(std::move(mixing.error()));
        }
    }

    if (j.contains("repeat")) {
        if (!j.at("repeat").is_object()) {
            errors.push_back(make_type_error("emitter.repeat", "an object"));
        } else {
        const auto& rj = j.at("repeat");
        RepeatSpec rs;
        if (rj.contains("count")) {
            auto count = parse_non_negative_int_value(rj.at("count"), "emitter.repeat.count");
            if (count.has_value()) {
                rs.count = *count;
            } else {
                errors.push_back(std::move(count.error()));
            }
        }
        if (rj.contains("interval_sec"))
            assign_double_value(rj.at("interval_sec"), "emitter.repeat.interval_sec", rs.interval_sec, errors);
        em.repeat = rs;
        }
    }

    if (j.contains("channel_id")) {
        assign_optional_string_value(j.at("channel_id"), "emitter.channel_id", em.channel_id, errors);
    }

    if (!errors.empty()) return std::unexpected(std::move(errors));
    return em;
}

static std::expected<ScenarioEvent, ErrorList> parse_event(const json& j) {
    ErrorList errors;
    ScenarioEvent evt;

    if (!j.is_object()) {
        return std::unexpected(ErrorList{make_type_error("event", "an object")});
    }

    if (!j.contains("target_device")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "event target_device is required"));
    } else {
        assign_string_value(j.at("target_device"), "event.target_device", evt.target_device, errors);
    }

    if (j.contains("time_sec")) assign_double_value(j.at("time_sec"), "event.time_sec", evt.time_sec, errors);

    if (!j.contains("type")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "event type is required"));
    } else {
        assign_string_value(j.at("type"), "event.type", evt.type, errors);
    }

    if (j.contains("payload")) {
        if (!j.at("payload").is_object()) {
            errors.push_back(make_type_error("event.payload", "an object"));
        } else {
            evt.payload = j.at("payload");
        }
    }

    if (!errors.empty()) return std::unexpected(std::move(errors));
    return evt;
}

static std::expected<Scenario, ErrorList> parse_scenario_from_json(const json& root) {
    ErrorList errors;
    Scenario scenario;

    if (!root.is_object()) {
        return std::unexpected(ErrorList{make_type_error("scenario", "an object")});
    }

    if (root.contains("metadata")) {
        if (!root.at("metadata").is_object()) {
            errors.push_back(make_type_error("metadata", "an object"));
        } else {
        const auto& m = root.at("metadata");
        if (m.contains("name")) assign_string_value(m.at("name"), "metadata.name", scenario.metadata.name, errors);
        if (m.contains("description")) {
            assign_optional_string_value(m.at("description"), "metadata.description",
                                         scenario.metadata.description, errors);
        }
        if (m.contains("version")) {
            assign_optional_string_value(m.at("version"), "metadata.version", scenario.metadata.version, errors);
        }
        }
    }

    if (root.contains("devices")) {
        if (!root.at("devices").is_array()) {
            errors.push_back(make_type_error("devices", "an array"));
        } else {
        for (const auto& dj : root.at("devices")) {
            auto dev_result = parse_device(dj);
            if (dev_result.has_value()) {
                scenario.devices.push_back(std::move(*dev_result));
            } else {
                for (auto& e : dev_result.error()) errors.push_back(std::move(e));
            }
        }
        }
    }

    if (root.contains("waveforms")) {
        if (!root.at("waveforms").is_array()) {
            errors.push_back(make_type_error("waveforms", "an array"));
        } else {
        for (const auto& wj : root.at("waveforms")) {
            auto wf_result = parse_waveform(wj);
            if (wf_result.has_value()) {
                scenario.waveforms.push_back(std::move(*wf_result));
            } else {
                for (auto& e : wf_result.error()) errors.push_back(std::move(e));
            }
        }
        }
    }

    if (root.contains("emitters")) {
        if (!root.at("emitters").is_array()) {
            errors.push_back(make_type_error("emitters", "an array"));
        } else {
        for (const auto& ej : root.at("emitters")) {
            auto em_result = parse_emitter(ej);
            if (em_result.has_value()) {
                scenario.emitters.push_back(std::move(*em_result));
            } else {
                for (auto& e : em_result.error()) errors.push_back(std::move(e));
            }
        }
        }
    }

    if (root.contains("events")) {
        if (!root.at("events").is_array()) {
            errors.push_back(make_type_error("events", "an array"));
        } else {
        for (const auto& evj : root.at("events")) {
            auto ev_result = parse_event(evj);
            if (ev_result.has_value()) {
                scenario.events.push_back(std::move(*ev_result));
            } else {
                for (auto& e : ev_result.error()) errors.push_back(std::move(e));
            }
        }
        }
    }

    if (root.contains("channels")) {
        if (!root.at("channels").is_array()) {
            errors.push_back(make_type_error("channels", "an array"));
        } else {
        for (const auto& cj : root.at("channels")) {
            if (!cj.is_object()) {
                errors.push_back(make_type_error("channel", "an object"));
                continue;
            }
            ChannelDef cd;
            if (!cj.contains("id")) {
                errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "channel.id is required"));
            } else {
                assign_string_value(cj.at("id"), "channel.id", cd.id, errors);
            }
            if (!cj.contains("device")) {
                errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "channel.device is required"));
            } else {
                assign_string_value(cj.at("device"), "channel.device", cd.device, errors);
            }
            if (!cj.contains("index")) {
                errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "channel.index is required"));
            } else {
                auto index = parse_uint32_value(cj.at("index"), "channel.index");
                if (index.has_value()) {
                    cd.index = *index;
                } else {
                    errors.push_back(std::move(index.error()));
                }
            }
            if (!cj.contains("rf")) {
                errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "channel.rf is required"));
            } else {
                auto rf = parse_rf_settings(cj.at("rf"));
                if (rf.has_value()) cd.rf = std::move(*rf);
                else for (auto& e : rf.error()) errors.push_back(std::move(e));
            }
            scenario.channel_defs.push_back(std::move(cd));
        }
        }
    }

    if (root.contains("sync_groups")) {
        if (!root.at("sync_groups").is_array()) {
            errors.push_back(make_type_error("sync_groups", "an array"));
        } else {
        for (const auto& sgj : root.at("sync_groups")) {
            if (!sgj.is_object()) {
                errors.push_back(make_type_error("sync_group", "an object"));
                continue;
            }
            SyncGroup sg;
            if (!sgj.contains("id")) {
                errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "sync_group.id is required"));
            } else {
                assign_string_value(sgj.at("id"), "sync_group.id", sg.id, errors);
            }
            if (!sgj.contains("mode")) {
                errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "sync_group.mode is required"));
            } else {
                assign_string_value(sgj.at("mode"), "sync_group.mode", sg.mode, errors);
            }
            if (!sgj.contains("channels")) {
                errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "sync_group.channels is required"));
            } else {
                if (!sgj.at("channels").is_array()) {
                    errors.push_back(make_type_error("sync_group.channels", "an array"));
                } else {
                for (const auto& cid : sgj.at("channels")) {
                    if (!cid.is_string()) {
                        errors.push_back(make_type_error("sync_group.channels", "an array of strings"));
                        continue;
                    }
                    sg.channels.push_back(cid.get<std::string>());
                }
                }
            }
            scenario.sync_groups.push_back(std::move(sg));
        }
        }
    }

    if (root.contains("reporting")) {
        if (!root.at("reporting").is_object()) {
            errors.push_back(make_type_error("reporting", "an object"));
        } else {
        const auto& r = root.at("reporting");
        if (r.contains("save_plan")) {
            assign_bool_value(r.at("save_plan"), "reporting.save_plan", scenario.reporting.save_plan, errors);
        }
        if (r.contains("save_metrics")) {
            assign_bool_value(r.at("save_metrics"), "reporting.save_metrics", scenario.reporting.save_metrics, errors);
        }
        }
    }

    if (root.contains("run")) {
        if (!root.at("run").is_object()) {
            errors.push_back(make_type_error("run", "an object"));
        } else {
        const auto& run_obj = root.at("run");
        if (run_obj.contains("mode")) {
            auto mode = parse_string_value(run_obj.at("mode"), "run.mode");
            if (mode.has_value()) {
                if (*mode == "replay") {
                    scenario.run.mode = RunMode::Replay;
                } else if (*mode == "realtime") {
                    scenario.run.mode = RunMode::Realtime;
                } else {
                    errors.push_back(make_error(ErrorCategory::Config, "E_INVALID_RUN_MODE",
                                                fmt::format("Invalid run mode '{}'; expected 'realtime' or 'replay'",
                                                            *mode)));
                }
            } else {
                errors.push_back(std::move(mode.error()));
            }
        }
        }
    }

    if (!errors.empty()) return std::unexpected(std::move(errors));
    return scenario;
}

std::expected<Scenario, ErrorList> parse_scenario_json(const std::string& json_str) {
    json root;
    try {
        root = json::parse(json_str);
    } catch (const json::parse_error& e) {
        ErrorList errors;
        errors.push_back(make_error(ErrorCategory::Config, "E_JSON_PARSE",
                                     fmt::format("JSON parse error: {}", e.what())));
        return std::unexpected(std::move(errors));
    }

    if (!root.is_object()) {
        ErrorList errors;
        errors.push_back(make_error(ErrorCategory::Config, "E_JSON_PARSE",
                                     "Scenario JSON must be an object"));
        return std::unexpected(std::move(errors));
    }

    try {
        return parse_scenario_from_json(root);
    } catch (const json::exception& e) {
        ErrorList errors;
        errors.push_back(make_error(ErrorCategory::Config, "E_JSON_TYPE",
                                     fmt::format("Scenario type error: {}", e.what())));
        return std::unexpected(std::move(errors));
    }
}

std::expected<Scenario, ErrorList> parse_scenario(const std::filesystem::path& json_path) {
    std::error_code fs_error;
    if (!std::filesystem::exists(json_path, fs_error)) {
        ErrorList errors;
        if (fs_error) {
            errors.push_back(make_error(ErrorCategory::Config, "E_FILE_READ",
                                         fmt::format("Cannot access file '{}': {}",
                                                     json_path.string(), fs_error.message())));
        } else {
            errors.push_back(make_error(ErrorCategory::Config, "E_FILE_NOT_FOUND",
                                         fmt::format("File not found: {}", json_path.string())));
        }
        return std::unexpected(std::move(errors));
    }

    std::ifstream ifs(json_path);
    if (!ifs.is_open()) {
        ErrorList errors;
        errors.push_back(make_error(ErrorCategory::Config, "E_FILE_READ",
                                     fmt::format("Cannot open file: {}", json_path.string())));
        return std::unexpected(std::move(errors));
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (ifs.bad()) {
        ErrorList errors;
        errors.push_back(make_error(ErrorCategory::Config, "E_FILE_READ",
                                     fmt::format("Failed while reading file: {}", json_path.string())));
        return std::unexpected(std::move(errors));
    }

    if (is_yaml_extension(json_path)) {
#ifdef ARCHERFISH_WITH_YAML
        try {
            auto yaml_root = YAML::Load(content);
            auto json_root = yaml_node_to_json(yaml_root);
            return parse_scenario_from_json(json_root);
        } catch (const json::exception& e) {
            ErrorList errors;
            errors.push_back(make_error(ErrorCategory::Config, "E_JSON_TYPE",
                                         fmt::format("Scenario type error: {}", e.what())));
            return std::unexpected(std::move(errors));
        } catch (const YAML::Exception& e) {
            ErrorList errors;
            errors.push_back(make_error(ErrorCategory::Config, "E_YAML_PARSE",
                                         fmt::format("YAML parse error: {}", e.what())));
            return std::unexpected(std::move(errors));
        }
#else
        ErrorList errors;
        errors.push_back(make_error(ErrorCategory::Config, "E_NO_YAML_SUPPORT",
                                     "YAML support not compiled in. Use JSON format."));
        return std::unexpected(std::move(errors));
#endif
    }

    return parse_scenario_json(content);
}

common::ErrorList resolve_waveform_refs(Scenario& scenario) {
    ErrorList errors;

    std::unordered_map<std::string, size_t> wf_index;
    for (size_t i = 0; i < scenario.waveforms.size(); ++i) {
        if (scenario.waveforms[i].id.has_value()) {
            wf_index[*scenario.waveforms[i].id] = i;
        }
    }

    for (auto& em : scenario.emitters) {
        if (em.waveform_ref.has_value() && !em.waveform.has_value()) {
            auto it = wf_index.find(*em.waveform_ref);
            if (it != wf_index.end()) {
                em.waveform = scenario.waveforms[it->second];
                em.waveform_ref = std::nullopt;
            } else {
                errors.push_back(make_error(ErrorCategory::Config, "E_UNRESOLVED_REF",
                                             fmt::format("Emitter '{}' references unknown waveform '{}'",
                                                          em.id, *em.waveform_ref)));
            }
        }
    }

    return errors;
}

} // namespace archerfish::scenario
