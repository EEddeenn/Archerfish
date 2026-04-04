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
    if (wf.target_power_dbm.has_value()) j["target_power_dbm"] = *wf.target_power_dbm;
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
    if (j.contains("target_power_dbm")) wf.target_power_dbm = j["target_power_dbm"].get<double>();
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
        nlohmann::json ij;
        const auto& imp = *e.impairments;
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
        j["impairments"] = ij;
    }
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
    if (j.contains("channel_id")) e.channel_id = j["channel_id"].get<std::string>();
    if (j.contains("mixing")) {
        auto m = j["mixing"].get<std::string>();
        e.mixing = (m == "additive") ? MixingMode::Additive : MixingMode::None;
    }
    if (j.contains("repeat")) {
        RepeatSpec rs;
        if (j["repeat"].contains("count")) rs.count = j["repeat"]["count"].get<int>();
        if (j["repeat"].contains("interval_sec")) rs.interval_sec = j["repeat"]["interval_sec"].get<double>();
        e.repeat = rs;
    }
    if (j.contains("impairments")) {
        ImpairmentSettings imp;
        const auto& ij = j["impairments"];
        if (ij.contains("cfo_hz")) imp.cfo_hz = ij["cfo_hz"].get<double>();
        if (ij.contains("phase_offset_rad")) imp.phase_offset_rad = ij["phase_offset_rad"].get<double>();
        if (ij.contains("iq_gain_imbalance_db")) imp.iq_gain_imbalance_db = ij["iq_gain_imbalance_db"].get<double>();
        if (ij.contains("iq_phase_imbalance_rad")) imp.iq_phase_imbalance_rad = ij["iq_phase_imbalance_rad"].get<double>();
        if (ij.contains("dc_offset_i")) imp.dc_offset_i = ij["dc_offset_i"].get<double>();
        if (ij.contains("dc_offset_q")) imp.dc_offset_q = ij["dc_offset_q"].get<double>();
        if (ij.contains("awgn_power")) imp.awgn_power = ij["awgn_power"].get<double>();
        if (ij.contains("amplitude_ripple_db")) imp.amplitude_ripple_db = ij["amplitude_ripple_db"].get<double>();
        if (ij.contains("amplitude_ripple_freq_hz")) imp.amplitude_ripple_freq_hz = ij["amplitude_ripple_freq_hz"].get<double>();
        if (ij.contains("delay_sec")) imp.delay_sec = ij["delay_sec"].get<double>();
        if (ij.contains("burst_dropout_rate")) imp.burst_dropout_rate = ij["burst_dropout_rate"].get<double>();
        if (ij.contains("burst_dropout_mean_burst_sec")) imp.burst_dropout_mean_burst_sec = ij["burst_dropout_mean_burst_sec"].get<double>();
        if (ij.contains("phase_noise_bandwidth_hz")) imp.phase_noise_bandwidth_hz = ij["phase_noise_bandwidth_hz"].get<double>();
        if (ij.contains("phase_noise_magnitude_rad")) imp.phase_noise_magnitude_rad = ij["phase_noise_magnitude_rad"].get<double>();
        if (ij.contains("phase_noise_psd_shape")) imp.phase_noise_psd_shape = ij["phase_noise_psd_shape"].get<std::string>();
        if (ij.contains("multipath_delay_samples")) imp.multipath_delay_samples = ij["multipath_delay_samples"].get<double>();
        if (ij.contains("multipath_amplitude")) imp.multipath_amplitude = ij["multipath_amplitude"].get<double>();
        if (ij.contains("fading_doppler_hz")) imp.fading_doppler_hz = ij["fading_doppler_hz"].get<double>();
        if (ij.contains("fading_type")) imp.fading_type = ij["fading_type"].get<std::string>();
        if (ij.contains("fading_k_factor")) imp.fading_k_factor = ij["fading_k_factor"].get<double>();
        if (ij.contains("pa_model")) imp.pa_model = ij["pa_model"].get<std::string>();
        if (ij.contains("pa_saturation")) imp.pa_saturation = ij["pa_saturation"].get<double>();
        if (ij.contains("pa_smoothness")) imp.pa_smoothness = ij["pa_smoothness"].get<double>();
        if (ij.contains("pa_phase_shift")) imp.pa_phase_shift = ij["pa_phase_shift"].get<double>();
        e.impairments = imp;
    }
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

nlohmann::json channel_def_to_json(const ChannelDef& c) {
    nlohmann::json j;
    j["id"] = c.id;
    j["device"] = c.device;
    j["index"] = c.index;
    j["rf"] = rf_settings_to_json(c.rf);
    return j;
}

std::expected<ChannelDef, common::ErrorList> channel_def_from_json(const nlohmann::json& j) {
    ChannelDef c;
    if (j.contains("id")) c.id = j["id"].get<std::string>();
    if (j.contains("device")) c.device = j["device"].get<std::string>();
    if (j.contains("index")) c.index = j["index"].get<uint32_t>();
    if (j.contains("rf")) {
        auto rf = rf_settings_from_json(j["rf"]);
        if (!rf.has_value()) return std::unexpected(std::move(rf).error());
        c.rf = *rf;
    }
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
    SyncGroup sg;
    if (j.contains("id")) sg.id = j["id"].get<std::string>();
    if (j.contains("mode")) sg.mode = j["mode"].get<std::string>();
    if (j.contains("channels") && j["channels"].is_array()) {
        for (const auto& cid : j["channels"]) {
            sg.channels.push_back(cid.get<std::string>());
        }
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
        ej["payload"] = evt.payload;
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
    if (j.contains("channels")) {
        for (const auto& cj : j["channels"]) {
            auto c = channel_def_from_json(cj);
            if (!c.has_value()) return std::unexpected(std::move(c).error());
            s.channel_defs.push_back(*c);
        }
    }
    if (j.contains("sync_groups")) {
        for (const auto& sgj : j["sync_groups"]) {
            auto sg = sync_group_from_json(sgj);
            if (!sg.has_value()) return std::unexpected(std::move(sg).error());
            s.sync_groups.push_back(*sg);
        }
    }
    if (j.contains("events")) {
        for (const auto& ej : j["events"]) {
            ScenarioEvent evt;
            if (ej.contains("target_device")) evt.target_device = ej["target_device"].get<std::string>();
            if (ej.contains("time_sec")) evt.time_sec = ej["time_sec"].get<double>();
            if (ej.contains("type")) evt.type = ej["type"].get<std::string>();
            if (ej.contains("payload")) evt.payload = ej["payload"];
            s.events.push_back(std::move(evt));
        }
    }
    if (j.contains("run")) {
        if (j["run"].contains("mode")) {
            auto mode = j["run"]["mode"].get<std::string>();
            s.run.mode = (mode == "replay") ? RunMode::Replay : RunMode::Realtime;
        }
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
            nlohmann::json rij;
            rij["emitter_id"] = ri.emitter_id;
            rij["waveform"] = waveform_def_to_json(ri.waveform);
            rij["start_sec"] = ri.start_sec;
            rij["duration_sec"] = ri.duration_sec;
            rij["sample_rate"] = ri.sample_rate;
            if (ri.resample_ratio.has_value()) rij["resample_ratio"] = *ri.resample_ratio;
            cp_render.push_back(rij);
        }
        cpj["render_instructions"] = cp_render;
        channel_plans_arr.push_back(cpj);
    }
    j["channel_plans"] = channel_plans_arr;

    j["run_mode"] = (plan.run_mode == RunMode::Replay) ? "replay" : "realtime";

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

    if (j.contains("mix_groups")) {
        for (const auto& mgj : j["mix_groups"]) {
            MixGroup mg;
            if (mgj.contains("device_id")) mg.device_id = mgj["device_id"].get<std::string>();
            if (mgj.contains("channel")) mg.channel = mgj["channel"].get<uint32_t>();
            if (mgj.contains("start_sec")) mg.start_sec = mgj["start_sec"].get<double>();
            if (mgj.contains("duration_sec")) mg.duration_sec = mgj["duration_sec"].get<double>();
            if (mgj.contains("estimated_peak_sum")) mg.estimated_peak_sum = mgj["estimated_peak_sum"].get<double>();
            if (mgj.contains("emitter_ids") && mgj["emitter_ids"].is_array()) {
                for (const auto& eid : mgj["emitter_ids"]) {
                    mg.emitter_ids.push_back(eid.get<std::string>());
                }
            }
            plan.mix_groups.push_back(std::move(mg));
        }
    }

    if (j.contains("channel_plans")) {
        for (const auto& cpj : j["channel_plans"]) {
            ChannelPlan cp;
            if (cpj.contains("channel_id")) cp.channel_id = cpj["channel_id"].get<std::string>();
            if (cpj.contains("channel_index")) cp.channel_index = cpj["channel_index"].get<uint32_t>();
            if (cpj.contains("rf")) {
                auto rf = rf_settings_from_json(cpj["rf"]);
                if (!rf.has_value()) return std::unexpected(std::move(rf).error());
                cp.rf = *rf;
            }
            if (cpj.contains("render_instructions")) {
                for (const auto& rij : cpj["render_instructions"]) {
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
                    if (rij.contains("resample_ratio")) ri.resample_ratio = rij["resample_ratio"].get<double>();
                    cp.render_instructions.push_back(std::move(ri));
                }
            }
            plan.channel_plans.push_back(std::move(cp));
        }
    }

    if (j.contains("run_mode")) {
        auto mode = j["run_mode"].get<std::string>();
        plan.run_mode = (mode == "replay") ? RunMode::Replay : RunMode::Realtime;
    }

    return plan;
}

} // namespace archerfish::scenario
