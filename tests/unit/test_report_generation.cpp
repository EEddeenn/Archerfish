#include <catch2/catch_test_macros.hpp>

#include "archerfish/reporting/report.hpp"
#include "archerfish/common/error.hpp"

using namespace archerfish::reporting;
using namespace archerfish::common;

TEST_CASE("Report JSON has marker_events field", "[reporting][report_gen]") {
    Report r;
    r.scenario_name = "test_markers";
    r.status = "completed";
    r.marker_events = {{"m1", 1.0, 1.01}, {"m2", 2.0, 2.02}};

    auto j = r.to_json();
    CHECK(j.contains("marker_events"));
    REQUIRE(j["marker_events"].is_array());
    REQUIRE(j["marker_events"].size() == 2);
    CHECK(j["marker_events"][0]["name"].get<std::string>() == "m1");
}

TEST_CASE("Report from_json restores marker_events", "[reporting][report_gen]") {
    auto j = R"({
        "scenario_name": "test",
        "scenario_hash": "",
        "status": "ok",
        "planned_start_sec": 0,
        "actual_start_sec": 0,
        "actual_stop_sec": 0,
        "actual_duration_sec": 0,
        "devices": [],
        "warnings": [],
        "errors": [],
        "artifact_paths": [],
        "marker_events": [
            {"name": "checkpoint1", "planned_time_sec": 5.0, "wall_clock_sec": 5.01}
        ]
    })"_json;

    auto r = Report::from_json(j);
    REQUIRE(r.marker_events.size() == 1);
    CHECK(r.marker_events[0].name == "checkpoint1");
    CHECK(r.marker_events[0].planned_time_sec == 5.0);
    CHECK(r.marker_events[0].wall_clock_sec == 5.01);
}

TEST_CASE("Report with errors serializes error details", "[reporting][report_gen]") {
    Report r;
    r.scenario_name = "error_test";
    r.status = "failed";
    r.errors = {
        {ErrorCategory::Execution, "E001", "timeout"},
        {ErrorCategory::Validation, "V002", "bad input"}
    };

    auto j = r.to_json();
    REQUIRE(j["errors"].size() == 2);
    CHECK(j["errors"][0]["code"].get<std::string>() == "E001");
    CHECK(j["errors"][1]["code"].get<std::string>() == "V002");
}

TEST_CASE("Report with devices serializes device info", "[reporting][report_gen]") {
    Report r;
    r.scenario_name = "device_test";
    r.status = "completed";
    r.devices = {{"usrp0", "B210", 0}, {"usrp1", "X310", 1}};

    auto j = r.to_json();
    REQUIRE(j["devices"].size() == 2);
    CHECK(j["devices"][0]["device_id"].get<std::string>() == "usrp0");
    CHECK(j["devices"][0]["device_type"].get<std::string>() == "B210");
    CHECK(j["devices"][0]["channel"].get<uint32_t>() == 0);
}

TEST_CASE("Report summary includes status info", "[reporting][report_gen]") {
    Report r;
    r.scenario_name = "summary_test";
    r.status = "completed";
    r.actual_duration_sec = 5.5;
    r.devices = {{"usrp0", "B210", 0}};
    r.warnings = {{ErrorCategory::QualityWarning, "W001", "clipping risk"}};
    r.errors = {};

    auto s = r.summary();
    REQUIRE_FALSE(s.empty());
    CHECK(s.find("summary_test") != std::string::npos);
    CHECK(s.find("completed") != std::string::npos);
}
