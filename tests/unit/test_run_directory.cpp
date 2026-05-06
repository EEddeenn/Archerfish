#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "archerfish/reporting/run_directory.hpp"
#include "archerfish/scenario/scenario.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using archerfish::dsp::WaveformType;

using namespace archerfish::reporting;
using namespace archerfish::scenario;

namespace {

const std::filesystem::path test_base = "/tmp/archerfish_test_runs";

void cleanup_test_dir() {
    std::filesystem::remove_all(test_base);
}

} // namespace

TEST_CASE("RunDirectory creates directory on construction", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory rd(test_base, "test_scenario");
        CHECK(std::filesystem::exists(rd.path()));
        CHECK(std::filesystem::is_directory(rd.path()));
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory name contains timestamp and scenario name", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory rd(test_base, "future_start_cw");
        auto dir_name = rd.path().filename().string();
        CHECK(dir_name.find("future_start_cw") != std::string::npos);
        CHECK(dir_name.size() >= 16);
        CHECK(dir_name[4] == '-');
        CHECK(dir_name[7] == '-');
        CHECK(dir_name[10] == 'T');
        CHECK(dir_name[dir_name.size() - 1] != '_');
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory path methods return correct filenames", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory rd(test_base, "test");
        CHECK(rd.scenario_path().filename() == "scenario.normalized.json");
        CHECK(rd.plan_path().filename() == "plan.json");
        CHECK(rd.metrics_path().filename() == "metrics.json");
        CHECK(rd.report_path().filename() == "report.json");
        CHECK(rd.log_path().filename() == "logs.txt");
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory save_scenario writes valid JSON", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory rd(test_base, "test");

        Scenario s;
        s.metadata.name = "test_scenario";
        s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0, 8e6, "TX/RX"}});

        rd.save_scenario(s);

        CHECK(std::filesystem::exists(rd.scenario_path()));

        std::ifstream f(rd.scenario_path());
        nlohmann::json j = nlohmann::json::parse(f);
        CHECK(j["metadata"]["name"].get<std::string>() == "test_scenario");
        CHECK(j["devices"].size() == 1);
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory save_plan writes valid JSON", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory rd(test_base, "test");

        Scenario s;
        s.metadata.name = "plan_test";
        s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
        EmitterDef em;
        em.id = "cw1";
        em.device = "usrp0";
        em.channel = 0;
        em.start_after_sec = 0.0;
        em.duration_sec = 5.0;
        em.waveform = WaveformDef{"cw_wf", WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
        s.emitters.push_back(em);

        Plan p;
        p.normalized_scenario = s;
        p.estimated_duration_sec = 5.0;

        rd.save_plan(p);

        CHECK(std::filesystem::exists(rd.plan_path()));

        std::ifstream f(rd.plan_path());
        nlohmann::json j = nlohmann::json::parse(f);
        CHECK(j.contains("normalized_scenario"));
        CHECK(j.contains("estimated_duration_sec"));
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory save_metrics writes valid JSON", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory rd(test_base, "test");

        Metrics m;
        m.underrun_count = 5;
        m.total_samples_sent = 100000;
        m.scenario_hash = "abc123";

        rd.save_metrics(m);

        CHECK(std::filesystem::exists(rd.metrics_path()));

        std::ifstream f(rd.metrics_path());
        nlohmann::json j = nlohmann::json::parse(f);
        CHECK(j["underrun_count"].get<size_t>() == 5);
        CHECK(j["total_samples_sent"].get<size_t>() == 100000);
        CHECK(j["scenario_hash"].get<std::string>() == "abc123");
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory save_report writes valid JSON", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory rd(test_base, "test");

        Report r;
        r.scenario_name = "test_report";
        r.status = "completed";
        r.actual_duration_sec = 10.0;

        rd.save_report(r);

        CHECK(std::filesystem::exists(rd.report_path()));

        std::ifstream f(rd.report_path());
        nlohmann::json j = nlohmann::json::parse(f);
        CHECK(j["scenario_name"].get<std::string>() == "test_report");
        CHECK(j["status"].get<std::string>() == "completed");
        CHECK(j["actual_duration_sec"].get<double>() == 10.0);
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory append_log adds lines to logs.txt", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory rd(test_base, "test");

        rd.append_log("First log line");
        rd.append_log("Second log line");

        CHECK(std::filesystem::exists(rd.log_path()));

        std::ifstream f(rd.log_path());
        std::string line;
        std::getline(f, line);
        CHECK(line == "First log line");
        std::getline(f, line);
        CHECK(line == "Second log line");
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory creates unique paths for repeated names", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory first(test_base, "same name");
        RunDirectory second(test_base, "same name");

        CHECK(first.path() != second.path());
        CHECK(std::filesystem::exists(first.path()));
        CHECK(std::filesystem::exists(second.path()));
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory falls back when scenario name sanitizes empty", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        RunDirectory rd(test_base, "###");
        CHECK(rd.path().filename().string().find("scenario") != std::string::npos);
        CHECK(std::filesystem::exists(rd.path()));
    }
    cleanup_test_dir();
}

TEST_CASE("RunDirectory truncates long sanitized scenario names", "[reporting][run_directory]") {
    cleanup_test_dir();
    {
        std::string long_name(1000, 'a');
        RunDirectory rd(test_base, long_name);
        CHECK(std::filesystem::exists(rd.path()));
        CHECK(rd.path().filename().string().size() <= 100);
    }
    cleanup_test_dir();
}
