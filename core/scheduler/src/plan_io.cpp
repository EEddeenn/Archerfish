#include "archerfish/scenario/plan_io.hpp"

#include <string>

#include "archerfish/dsp/waveform_type.hpp"

namespace archerfish::scenario {

namespace {

std::string timeline_event_type_to_string(TimelineEventType type) {
    switch (type) {
        case TimelineEventType::EmitterStart: return "EmitterStart";
        case TimelineEventType::EmitterStop: return "EmitterStop";
        case TimelineEventType::GainChange: return "GainChange";
        case TimelineEventType::FreqChange: return "FreqChange";
        case TimelineEventType::Marker: return "Marker";
    }
    return "Unknown";
}

std::expected<TimelineEventType, common::ErrorList> string_to_timeline_event_type(const std::string& s) {
    if (s == "EmitterStart") return TimelineEventType::EmitterStart;
    if (s == "EmitterStop") return TimelineEventType::EmitterStop;
    if (s == "GainChange") return TimelineEventType::GainChange;
    if (s == "FreqChange") return TimelineEventType::FreqChange;
    if (s == "Marker") return TimelineEventType::Marker;
    return std::unexpected(common::ErrorList{
        {common::ErrorCategory::Planning, "E_PLAN_IO_BAD_EVENT_TYPE", "Unknown timeline event type: " + s}
    });
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
    RfSettings rf;
    if (j.contains("freq_hz")) rf.freq_hz = j["freq_hz"].get<double>();
    if (j.contains("rate_sps")) rf.rate_sps = j["rate_sps"].get<double>();
    if (j.contains("gain_db")) rf.gain_db = j["gain_db"].get<double>();
    if (j.contains("bandwidth_hz")) rf.bandwidth_hz = j["bandwidth_hz"].get<double>();
    if (j.contains("antenna")) rf.antenna = j["antenna"].get<std::string>();
    return rf;
}

nlohmann::json waveform_def_to_json(const WaveformDef& wf) {
    nlohmann::json j;
    if (wf.id.has_value()) j["id"] = *wf.id;
    j["type"] = dsp::to_string(wf.type);
    j["params"] = wf.params;
    return j;
}

std::expected<WaveformDef, common::ErrorList> waveform_def_from_json(const nlohmann::json& j) {
    WaveformDef wf;
    if (j.contains("id")) wf.id = j["id"].get<std::string>();
    if (j.contains("type")) {
        auto type_str = j["type"].get<std::string>();
        auto type_result = dsp::waveform_type_from_string(type_str);
        if (!type_result.has_value()) {
            return std::unexpected(common::ErrorList{
                {common::ErrorCategory::Planning, "E_PLAN_IO_BAD_WAVEFORM_TYPE",
                 "Unknown waveform type in plan JSON: " + type_str}});
        }
        wf.type = *type_result;
    }
    if (j.contains("params")) wf.params = j["params"];
    return wf;
}

nlohmann::json metadata_to_json(const Metadata& m) {
    nlohmann::json j;
    j["name"] = m.name;
    if (m.description.has_value()) j["description"] = *m.description;
    if (m.version.has_value()) j["version"] = *m.version;
    return j;
}

std::expected<Metadata, common::ErrorList> metadata_from_json(const nlohmann::json& j) {
    Metadata m;
    if (j.contains("name")) m.name = j["name"].get<std::string>();
    if (j.contains("description")) m.description = j["description"].get<std::string>();
    if (j.contains("version")) m.version = j["version"].get<std::string>();
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
    DeviceDef d;
    if (j.contains("id")) d.id = j["id"].get<std::string>();
    if (j.contains("channel")) d.channel = j["channel"].get<uint32_t>();
    if (j.contains("rf")) {
        auto rf = rf_settings_from_json(j["rf"]);
        if (!rf.has_value()) return std::unexpected(std::move(rf).error());
        d.rf = *rf;
    }
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
    return j;
}

std::expected<EmitterDef, common::ErrorList> emitter_def_from_json(const nlohmann::json& j) {
    EmitterDef e;
    if (j.contains("id")) e.id = j["id"].get<std::string>();
    if (j.contains("device")) e.device = j["device"].get<std::string>();
    if (j.contains("channel")) e.channel = j["channel"].get<uint32_t>();
    if (j.contains("start_after_sec")) e.start_after_sec = j["start_after_sec"].get<double>();
    if (j.contains("duration_sec")) e.duration_sec = j["duration_sec"].get<double>();
    if (j.contains("waveform")) {
        auto wf = waveform_def_from_json(j["waveform"]);
        if (!wf.has_value()) return std::unexpected(std::move(wf).error());
        e.waveform = *wf;
    }
    if (j.contains("waveform_ref")) e.waveform_ref = j["waveform_ref"].get<std::string>();
    return e;
}

nlohmann::json reporting_config_to_json(const ReportingConfig& r) {
    return nlohmann::json{{"save_plan", r.save_plan}, {"save_metrics", r.save_metrics}};
}

ReportingConfig reporting_config_from_json(const nlohmann::json& j) {
    ReportingConfig r;
    if (j.contains("save_plan")) r.save_plan = j["save_plan"].get<bool>();
    if (j.contains("save_metrics")) r.save_metrics = j["save_metrics"].get<bool>();
    return r;
}

nlohmann::json scenario_to_json(const Scenario& s) {
    nlohmann::json j;
    j["metadata"] = metadata_to_json(s.metadata);
    for (const auto& d : s.devices) j["devices"].push_back(device_def_to_json(d));
    for (const auto& w : s.waveforms) j["waveforms"].push_back(waveform_def_to_json(w));
    for (const auto& e : s.emitters) j["emitters"].push_back(emitter_def_to_json(e));
    j["reporting"] = reporting_config_to_json(s.reporting);
    return j;
}

std::expected<Scenario, common::ErrorList> scenario_from_json(const nlohmann::json& j) {
    Scenario s;
    if (j.contains("metadata")) {
        auto m = metadata_from_json(j["metadata"]);
        if (!m.has_value()) return std::unexpected(std::move(m).error());
        s.metadata = *m;
    }
    if (j.contains("devices")) {
        for (const auto& dj : j["devices"]) {
            auto d = device_def_from_json(dj);
            if (!d.has_value()) return std::unexpected(std::move(d).error());
            s.devices.push_back(*d);
        }
    }
    if (j.contains("waveforms")) {
        for (const auto& wj : j["waveforms"]) {
            auto w = waveform_def_from_json(wj);
            if (!w.has_value()) return std::unexpected(std::move(w).error());
            s.waveforms.push_back(*w);
        }
    }
    if (j.contains("emitters")) {
        for (const auto& ej : j["emitters"]) {
            auto e = emitter_def_from_json(ej);
            if (!e.has_value()) return std::unexpected(std::move(e).error());
            s.emitters.push_back(*e);
        }
    }
    if (j.contains("reporting")) {
        s.reporting = reporting_config_from_json(j["reporting"]);
    }
    return s;
}

nlohmann::json error_to_json(const common::Error& err) {
    return nlohmann::json{
        {"category", common::category_to_string(err.category)},
        {"code", err.code},
        {"message", err.message}};
}

common::Error error_from_json(const nlohmann::json& j) {
    common::Error err;
    err.category = common::category_from_string(j.value("category", "Config"));
    err.code = j.value("code", "");
    err.message = j.value("message", "");
    return err;
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
        nlohmann::json evj;
        evj["type"] = timeline_event_type_to_string(ev.type);
        evj["time_sec"] = ev.time_sec;
        evj["target_id"] = ev.target_id;
        evj["payload"] = ev.payload;
        timeline_arr.push_back(evj);
    }
    j["timeline"] = timeline_arr;

    nlohmann::json render_arr = nlohmann::json::array();
    for (const auto& ri : plan.render_instructions) {
        nlohmann::json rij;
        rij["emitter_id"] = ri.emitter_id;
        rij["waveform"] = waveform_def_to_json(ri.waveform);
        rij["start_sec"] = ri.start_sec;
        rij["duration_sec"] = ri.duration_sec;
        rij["sample_rate"] = ri.sample_rate;
        if (ri.resample_ratio.has_value()) {
            rij["resample_ratio"] = *ri.resample_ratio;
        }
        render_arr.push_back(rij);
    }
    j["render_instructions"] = render_arr;

    nlohmann::json warnings_arr = nlohmann::json::array();
    for (const auto& w : plan.warnings) {
        warnings_arr.push_back(error_to_json(w));
    }
    j["warnings"] = warnings_arr;

    j["estimated_duration_sec"] = plan.estimated_duration_sec;

    return j;
}

std::expected<Plan, common::ErrorList> plan_from_json(const nlohmann::json& j) {
    using common::Error;
    using common::ErrorCategory;
    using common::ErrorList;

    Plan plan;

    if (j.contains("normalized_scenario")) {
        auto s = scenario_from_json(j["normalized_scenario"]);
        if (!s.has_value()) return std::unexpected(std::move(s).error());
        plan.normalized_scenario = *s;
    }

    if (j.contains("channels")) {
        for (const auto& chj : j["channels"]) {
            ChannelBinding ch;
            if (chj.contains("device_id")) ch.device_id = chj["device_id"].get<std::string>();
            if (chj.contains("channel_index")) ch.channel_index = chj["channel_index"].get<uint32_t>();
            if (chj.contains("rf")) {
                auto rf = rf_settings_from_json(chj["rf"]);
                if (!rf.has_value()) return std::unexpected(std::move(rf).error());
                ch.rf = *rf;
            }
            plan.channels.push_back(std::move(ch));
        }
    }

    if (j.contains("timeline")) {
        for (const auto& evj : j["timeline"]) {
            TimelineEvent ev;
            if (evj.contains("type")) {
                auto type = string_to_timeline_event_type(evj["type"].get<std::string>());
                if (!type.has_value()) return std::unexpected(std::move(type).error());
                ev.type = *type;
            }
            if (evj.contains("time_sec")) ev.time_sec = evj["time_sec"].get<double>();
            if (evj.contains("target_id")) ev.target_id = evj["target_id"].get<std::string>();
            if (evj.contains("payload")) ev.payload = evj["payload"];
            plan.timeline.push_back(std::move(ev));
        }
    }

    if (j.contains("render_instructions")) {
        for (const auto& rij : j["render_instructions"]) {
            RenderInstruction ri;
            if (rij.contains("emitter_id")) ri.emitter_id = rij["emitter_id"].get<std::string>();
            if (rij.contains("waveform")) {
                auto wf = waveform_def_from_json(rij["waveform"]);
                if (!wf.has_value()) return std::unexpected(std::move(wf).error());
                ri.waveform = *wf;
            }
            if (rij.contains("start_sec")) ri.start_sec = rij["start_sec"].get<double>();
            if (rij.contains("duration_sec")) ri.duration_sec = rij["duration_sec"].get<double>();
            if (rij.contains("sample_rate")) ri.sample_rate = rij["sample_rate"].get<double>();
            if (rij.contains("resample_ratio")) {
                ri.resample_ratio = rij["resample_ratio"].get<double>();
            }
            plan.render_instructions.push_back(std::move(ri));
        }
    }

    if (j.contains("warnings")) {
        for (const auto& wj : j["warnings"]) {
            plan.warnings.push_back(error_from_json(wj));
        }
    }

    if (j.contains("estimated_duration_sec")) {
        plan.estimated_duration_sec = j["estimated_duration_sec"].get<double>();
    }

    return plan;
}

} // namespace archerfish::scenario
