#include "archerfish/scenario/parser.hpp"

#include <fstream>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <fmt/format.h>

#include "archerfish/dsp/waveform_type.hpp"

namespace archerfish::scenario {

using json = nlohmann::json;
using namespace archerfish::common;

static Error make_error(ErrorCategory cat, std::string code, std::string message) {
    return Error{cat, std::move(code), std::move(message)};
}

static std::expected<RfSettings, ErrorList> parse_rf_settings(const json& j) {
    ErrorList errors;
    RfSettings rf;

    if (!j.contains("freq_hz")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "rf.freq_hz is required"));
    } else {
        rf.freq_hz = j.at("freq_hz").get<double>();
    }

    if (!j.contains("rate_sps")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "rf.rate_sps is required"));
    } else {
        rf.rate_sps = j.at("rate_sps").get<double>();
    }

    if (!j.contains("gain_db")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "rf.gain_db is required"));
    } else {
        rf.gain_db = j.at("gain_db").get<double>();
    }

    if (j.contains("bandwidth_hz")) rf.bandwidth_hz = j.at("bandwidth_hz").get<double>();
    if (j.contains("antenna")) rf.antenna = j.at("antenna").get<std::string>();

    if (!errors.empty()) return std::unexpected(std::move(errors));
    return rf;
}

static std::expected<DeviceDef, ErrorList> parse_device(const json& j) {
    ErrorList errors;
    DeviceDef dev;

    if (!j.contains("id")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "device id is required"));
    } else {
        dev.id = j.at("id").get<std::string>();
    }

    if (j.contains("channel")) dev.channel = j.at("channel").get<uint32_t>();

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
    WaveformDef wf;
    wf.id = std::move(id);

    auto type_str = j.at("type").get<std::string>();
    auto type_result = dsp::waveform_type_from_string(type_str);
    if (!type_result.has_value()) {
        return std::unexpected(ErrorList{
            make_error(ErrorCategory::Config, "E_INVALID_WAVEFORM_TYPE", type_result.error())});
    }
    wf.type = *type_result;

    json params = j;
    params.erase("type");
    if (wf.id.has_value()) params.erase("id");
    wf.params = params;

    return wf;
}

static std::expected<ImpairmentSettings, ErrorList> parse_impairments(const json& j) {
    ImpairmentSettings imp;
    if (j.contains("cfo_hz")) imp.cfo_hz = j.at("cfo_hz").get<double>();
    if (j.contains("phase_offset_rad")) imp.phase_offset_rad = j.at("phase_offset_rad").get<double>();
    if (j.contains("iq_gain_imbalance_db")) imp.iq_gain_imbalance_db = j.at("iq_gain_imbalance_db").get<double>();
    if (j.contains("iq_phase_imbalance_rad")) imp.iq_phase_imbalance_rad = j.at("iq_phase_imbalance_rad").get<double>();
    if (j.contains("dc_offset_i")) imp.dc_offset_i = j.at("dc_offset_i").get<double>();
    if (j.contains("dc_offset_q")) imp.dc_offset_q = j.at("dc_offset_q").get<double>();
    if (j.contains("awgn_power")) imp.awgn_power = j.at("awgn_power").get<double>();
    if (j.contains("amplitude_ripple_db"))
        imp.amplitude_ripple_db = j.at("amplitude_ripple_db").get<double>();
    if (j.contains("amplitude_ripple_freq_hz"))
        imp.amplitude_ripple_freq_hz = j.at("amplitude_ripple_freq_hz").get<double>();
    if (j.contains("delay_sec"))
        imp.delay_sec = j.at("delay_sec").get<double>();
    if (j.contains("burst_dropout_rate"))
        imp.burst_dropout_rate = j.at("burst_dropout_rate").get<double>();
    return imp;
}

static std::expected<EmitterDef, ErrorList> parse_emitter(const json& j) {
    ErrorList errors;
    EmitterDef em;

    if (!j.contains("id")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "emitter id is required"));
    } else {
        em.id = j.at("id").get<std::string>();
    }

    if (!j.contains("device")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD",
                                     fmt::format("emitter '{}' missing device", em.id)));
    } else {
        em.device = j.at("device").get<std::string>();
    }

    if (j.contains("channel")) em.channel = j.at("channel").get<uint32_t>();
    if (j.contains("start_after_sec")) em.start_after_sec = j.at("start_after_sec").get<double>();
    if (j.contains("duration_sec")) em.duration_sec = j.at("duration_sec").get<double>();

    if (j.contains("waveform") && j.at("waveform").is_object()) {
        auto wf_result = parse_waveform(j.at("waveform"));
        if (wf_result.has_value()) {
            em.waveform = std::move(*wf_result);
        } else {
            for (auto& e : wf_result.error()) errors.push_back(std::move(e));
        }
    }
    if (j.contains("waveform_ref")) {
        em.waveform_ref = j.at("waveform_ref").get<std::string>();
    }

    if (j.contains("impairments") && j.at("impairments").is_object()) {
        auto imp_result = parse_impairments(j.at("impairments"));
        if (imp_result.has_value()) {
            em.impairments = std::move(*imp_result);
        } else {
            for (auto& e : imp_result.error()) errors.push_back(std::move(e));
        }
    }

    if (j.contains("mixing")) {
        auto mixing_str = j.at("mixing").get<std::string>();
        if (mixing_str == "additive") {
            em.mixing = MixingMode::Additive;
        } else {
            em.mixing = MixingMode::None;
        }
    }

    if (j.contains("repeat") && j.at("repeat").is_object()) {
        const auto& rj = j.at("repeat");
        RepeatSpec rs;
        if (rj.contains("count")) rs.count = rj.at("count").get<int>();
        if (rj.contains("interval_sec")) rs.interval_sec = rj.at("interval_sec").get<double>();
        em.repeat = rs;
    }

    if (!errors.empty()) return std::unexpected(std::move(errors));
    return em;
}

static std::expected<ScenarioEvent, ErrorList> parse_event(const json& j) {
    ErrorList errors;
    ScenarioEvent evt;

    if (!j.contains("target_device")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "event target_device is required"));
    } else {
        evt.target_device = j.at("target_device").get<std::string>();
    }

    if (j.contains("time_sec")) evt.time_sec = j.at("time_sec").get<double>();

    if (!j.contains("type")) {
        errors.push_back(make_error(ErrorCategory::Config, "E_MISSING_FIELD", "event type is required"));
    } else {
        evt.type = j.at("type").get<std::string>();
    }

    if (j.contains("payload") && j.at("payload").is_object()) {
        evt.payload = j.at("payload");
    }

    if (!errors.empty()) return std::unexpected(std::move(errors));
    return evt;
}

static std::expected<Scenario, ErrorList> parse_scenario_from_json(const json& root) {
    ErrorList errors;
    Scenario scenario;

    if (root.contains("metadata") && root.at("metadata").is_object()) {
        const auto& m = root.at("metadata");
        if (m.contains("name")) scenario.metadata.name = m.at("name").get<std::string>();
        if (m.contains("description")) scenario.metadata.description = m.at("description").get<std::string>();
        if (m.contains("version")) scenario.metadata.version = m.at("version").get<std::string>();
    }

    if (root.contains("devices") && root.at("devices").is_array()) {
        for (const auto& dj : root.at("devices")) {
            auto dev_result = parse_device(dj);
            if (dev_result.has_value()) {
                scenario.devices.push_back(std::move(*dev_result));
            } else {
                for (auto& e : dev_result.error()) errors.push_back(std::move(e));
            }
        }
    }

    if (root.contains("waveforms") && root.at("waveforms").is_array()) {
        for (const auto& wj : root.at("waveforms")) {
            std::optional<std::string> wf_id;
            if (wj.contains("id")) wf_id = wj.at("id").get<std::string>();
            auto wf_result = parse_waveform(wj, std::move(wf_id));
            if (wf_result.has_value()) {
                scenario.waveforms.push_back(std::move(*wf_result));
            } else {
                for (auto& e : wf_result.error()) errors.push_back(std::move(e));
            }
        }
    }

    if (root.contains("emitters") && root.at("emitters").is_array()) {
        for (const auto& ej : root.at("emitters")) {
            auto em_result = parse_emitter(ej);
            if (em_result.has_value()) {
                scenario.emitters.push_back(std::move(*em_result));
            } else {
                for (auto& e : em_result.error()) errors.push_back(std::move(e));
            }
        }
    }

    if (root.contains("events") && root.at("events").is_array()) {
        for (const auto& evj : root.at("events")) {
            auto ev_result = parse_event(evj);
            if (ev_result.has_value()) {
                scenario.events.push_back(std::move(*ev_result));
            } else {
                for (auto& e : ev_result.error()) errors.push_back(std::move(e));
            }
        }
    }

    if (root.contains("reporting") && root.at("reporting").is_object()) {
        const auto& r = root.at("reporting");
        if (r.contains("save_plan")) scenario.reporting.save_plan = r.at("save_plan").get<bool>();
        if (r.contains("save_metrics")) scenario.reporting.save_metrics = r.at("save_metrics").get<bool>();
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

    return parse_scenario_from_json(root);
}

std::expected<Scenario, ErrorList> parse_scenario(const std::filesystem::path& json_path) {
    if (!std::filesystem::exists(json_path)) {
        ErrorList errors;
        errors.push_back(make_error(ErrorCategory::Config, "E_FILE_NOT_FOUND",
                                     fmt::format("File not found: {}", json_path.string())));
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
