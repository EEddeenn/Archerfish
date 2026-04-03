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
    write_json_file(scenario_path(), scenario::scenario_to_json(scenario));
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
