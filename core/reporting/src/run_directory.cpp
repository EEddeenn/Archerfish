#include "archerfish/reporting/run_directory.hpp"

#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <stdexcept>
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
    constexpr size_t kMaxScenarioNameLength = 80;
    if (result.size() > kMaxScenarioNameLength) {
        result.resize(kMaxScenarioNameLength);
    }
    while (!result.empty() && (result.back() == '_' || result.back() == '-')) {
        result.pop_back();
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
    if (!f) {
        throw std::runtime_error("Cannot open run artifact for writing: " + p.string());
    }
    f << j.dump(2) << std::endl;
    if (!f) {
        throw std::runtime_error("Failed to write run artifact: " + p.string());
    }
}

} // namespace

RunDirectory::RunDirectory(const std::filesystem::path& base_path,
                           const std::string& scenario_name) {
    auto timestamp = make_timestamp();
    auto safe_name = sanitize_scenario_name(scenario_name);
    if (safe_name.empty()) {
        safe_name = "scenario";
    }

    const auto runs_path = base_path / "runs";
    std::error_code fs_error;
    std::filesystem::create_directories(runs_path, fs_error);
    if (fs_error) {
        throw std::runtime_error("Failed to create runs directory '" + runs_path.string() + "': " + fs_error.message());
    }
    if (!std::filesystem::is_directory(runs_path, fs_error) || fs_error) {
        throw std::runtime_error("Failed to create runs directory: " + runs_path.string());
    }

    std::string dir_name = timestamp + "_" + safe_name;
    for (size_t suffix = 0;; ++suffix) {
        dir_path_ = runs_path / (suffix == 0 ? dir_name : dir_name + "_" + std::to_string(suffix));
        if (std::filesystem::create_directory(dir_path_, fs_error)) {
            break;
        }
        if (fs_error) {
            throw std::runtime_error("Failed to create run directory '" + dir_path_.string() + "': " +
                                     fs_error.message());
        }
    }
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
    if (!f) {
        throw std::runtime_error("Cannot open run log for writing: " + log_path().string());
    }
    f << message << std::endl;
    if (!f) {
        throw std::runtime_error("Failed to write run log: " + log_path().string());
    }
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
