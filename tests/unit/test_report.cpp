#include <catch2/catch_test_macros.hpp>

#include "archerfish/reporting/report.hpp"

using namespace archerfish::reporting;
using namespace archerfish::common;

TEST_CASE("Report default construction", "[reporting][report]") {
    Report r;
    CHECK(r.scenario_name.empty());
    CHECK(r.scenario_hash.empty());
    CHECK(r.status.empty());
    CHECK(r.planned_start_sec == 0.0);
    CHECK(r.actual_start_sec == 0.0);
    CHECK(r.actual_stop_sec == 0.0);
    CHECK(r.actual_duration_sec == 0.0);
    CHECK(r.devices.empty());
    CHECK(r.warnings.empty());
    CHECK(r.errors.empty());
    CHECK(r.artifact_paths.empty());
}

TEST_CASE("Report to_json contains expected keys", "[reporting][report]") {
    Report r;
    r.scenario_name = "test_scenario";
    r.status = "completed";
    auto j = r.to_json();

    CHECK(j.contains("scenario_name"));
    CHECK(j.contains("scenario_hash"));
    CHECK(j.contains("status"));
    CHECK(j.contains("planned_start_sec"));
    CHECK(j.contains("actual_start_sec"));
    CHECK(j.contains("actual_stop_sec"));
    CHECK(j.contains("actual_duration_sec"));
    CHECK(j.contains("devices"));
    CHECK(j.contains("warnings"));
    CHECK(j.contains("errors"));
    CHECK(j.contains("artifact_paths"));
}

TEST_CASE("Report round-trip preserves all fields", "[reporting][report]") {
    Report original;
    original.scenario_name = "future_start_cw";
    original.scenario_hash = "deadbeef";
    original.status = "completed";
    original.planned_start_sec = 1000.0;
    original.actual_start_sec = 1000.1;
    original.actual_stop_sec = 1010.0;
    original.actual_duration_sec = 9.9;
    original.devices = {{"usrp0", "B210", 0}, {"usrp1", "X310", 1}};
    original.warnings = {{ErrorCategory::QualityWarning, "W001", "low power"}};
    original.errors = {{ErrorCategory::Execution, "E001", "timeout"}};
    original.artifact_paths = {"/tmp/run1/scenario.json", "/tmp/run1/plan.json"};

    auto j = original.to_json();
    auto restored = Report::from_json(j);

    CHECK(restored.scenario_name == original.scenario_name);
    CHECK(restored.scenario_hash == original.scenario_hash);
    CHECK(restored.status == original.status);
    CHECK(restored.planned_start_sec == original.planned_start_sec);
    CHECK(restored.actual_start_sec == original.actual_start_sec);
    CHECK(restored.actual_stop_sec == original.actual_stop_sec);
    CHECK(restored.actual_duration_sec == original.actual_duration_sec);
    REQUIRE(restored.devices.size() == 2);
    CHECK(restored.devices[0].device_id == "usrp0");
    CHECK(restored.devices[0].device_type == "B210");
    CHECK(restored.devices[0].channel == 0);
    CHECK(restored.devices[1].device_id == "usrp1");
    CHECK(restored.devices[1].device_type == "X310");
    CHECK(restored.devices[1].channel == 1);
    REQUIRE(restored.warnings.size() == 1);
    CHECK(restored.warnings[0].code == "W001");
    CHECK(restored.warnings[0].message == "low power");
    REQUIRE(restored.errors.size() == 1);
    CHECK(restored.errors[0].code == "E001");
    CHECK(restored.errors[0].message == "timeout");
    REQUIRE(restored.artifact_paths.size() == 2);
    CHECK(restored.artifact_paths[0] == "/tmp/run1/scenario.json");
    CHECK(restored.artifact_paths[1] == "/tmp/run1/plan.json");
}

TEST_CASE("Report summary produces non-empty string with key info", "[reporting][report]") {
    Report r;
    r.scenario_name = "test_scenario";
    r.status = "completed";
    r.actual_duration_sec = 10.5;
    r.devices = {{"usrp0", "B210", 0}};
    r.warnings = {{ErrorCategory::QualityWarning, "W001", "test"}};
    r.errors = {{ErrorCategory::Execution, "E001", "fail"}};
    r.artifact_paths = {"/tmp/run/scenario.json"};

    auto s = r.summary();
    REQUIRE_FALSE(s.empty());
    CHECK(s.find("test_scenario") != std::string::npos);
    CHECK(s.find("completed") != std::string::npos);
    CHECK(s.find("10.500") != std::string::npos);
}
