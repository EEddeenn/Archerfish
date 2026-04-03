#include "archerfish/reporting/run_directory.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <string>

#include <nlohmann/json.hpp>

#include "archerfish/scenario/plan_io.hpp"

namespace archerfish::reporting {

namespace {

std::string sanitize_scenario_name(const std::string& name) {
    std::string result;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            result += c;
        } else if (c == ' ') {
            result += '_';
        }
    }
    return result;
}

std::string make_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_r(&time_t_now, &tm_buf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H%M%SZ", &tm_buf);
    return buf;
}

nlohmann::json metadata_to_json(const scenario::Metadata& m) {
    nlohmann::json j;
    j["name"] = m.name;
    if (m.description.has_value()) j["description"] = *m.description;
    if (m.version.has_value()) j["version"] = *m.version;
    return j;
}

nlohmann::json rf_settings_to_json(const scenario::RfSettings& rf) {
    nlohmann::json j;
    j["freq_hz"] = rf.freq_hz;
    j["rate_sps"] = rf.rate_sps;
    j["gain_db"] = rf.gain_db;
    if (rf.bandwidth_hz.has_value()) j["bandwidth_hz"] = *rf.bandwidth_hz;
    if (rf.antenna.has_value()) j["antenna"] = *rf.antenna;
    return j;
}

nlohmann::json device_def_to_json(const scenario::DeviceDef& d) {
    nlohmann::json j;
    j["id"] = d.id;
    if (d.channel.has_value()) j["channel"] = *d.channel;
    j["rf"] = rf_settings_to_json(d.rf);
    return j;
}

nlohmann::json waveform_def_to_json(const scenario::WaveformDef& wf) {
    nlohmann::json j;
    if (wf.id.has_value()) j["id"] = *wf.id;
    j["type"] = wf.type;
    j["params"] = wf.params;
    return j;
}

nlohmann::json emitter_def_to_json(const scenario::EmitterDef& e) {
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

nlohmann::json scenario_to_json(const scenario::Scenario& s) {
    nlohmann::json j;
    j["metadata"] = metadata_to_json(s.metadata);
    for (const auto& d : s.devices) j["devices"].push_back(device_def_to_json(d));
    for (const auto& w : s.waveforms) j["waveforms"].push_back(waveform_def_to_json(w));
    for (const auto& e : s.emitters) j["emitters"].push_back(emitter_def_to_json(e));
    j["reporting"] = nlohmann::json{{"save_plan", s.reporting.save_plan}, {"save_metrics", s.reporting.save_metrics}};
    return j;
}

void write_json_file(const std::filesystem::path& p, const nlohmann::json& j) {
    std::ofstream f(p);
    f << j.dump(2) << std::endl;
}

} // namespace

RunDirectory::RunDirectory(const std::filesystem::path& base_path,
                           const std::string& scenario_name) {
    auto timestamp = make_timestamp();
    auto safe_name = sanitize_scenario_name(scenario_name);
    std::string dir_name = timestamp + "_" + safe_name;
    dir_path_ = base_path / "runs" / dir_name;
    std::filesystem::create_directories(dir_path_);
}

const std::filesystem::path& RunDirectory::path() const {
    return dir_path_;
}

void RunDirectory::save_scenario(const scenario::Scenario& scenario) {
    write_json_file(scenario_path(), scenario_to_json(scenario));
}

void RunDirectory::save_plan(const scenario::Plan& plan) {
    write_json_file(plan_path(), scenario::plan_to_json(plan));
}

void RunDirectory::save_metrics(const Metrics& metrics) {
    write_json_file(metrics_path(), metrics.to_json());
}

void RunDirectory::save_report(const Report& report) {
    write_json_file(report_path(), report.to_json());
}

void RunDirectory::append_log(const std::string& message) {
    std::ofstream f(log_path(), std::ios::app);
    f << message << std::endl;
}

std::filesystem::path RunDirectory::scenario_path() const {
    return dir_path_ / "scenario.normalized.json";
}

std::filesystem::path RunDirectory::plan_path() const {
    return dir_path_ / "plan.json";
}

std::filesystem::path RunDirectory::metrics_path() const {
    return dir_path_ / "metrics.json";
}

std::filesystem::path RunDirectory::report_path() const {
    return dir_path_ / "report.json";
}

std::filesystem::path RunDirectory::log_path() const {
    return dir_path_ / "logs.txt";
}

} // namespace archerfish::reporting
