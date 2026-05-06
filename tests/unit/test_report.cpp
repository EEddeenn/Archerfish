#include <catch2/catch_test_macros.hpp>

#include "archerfish/reporting/report.hpp"

#include <limits>
#include <stdexcept>

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

TEST_CASE("Report rejects invalid timing fields", "[reporting][report]") {
    Report r;
    r.scenario_name = "test_scenario";
    r.status = "completed";
    r.actual_start_sec = 10.0;
    r.actual_stop_sec = 12.5;
    r.actual_duration_sec = 2.5;
    r.marker_events.push_back({"m1", 1.0, 2.0});
    auto j = r.to_json();

    auto bad_duration = j;
    bad_duration["actual_duration_sec"] = -1.0;
    CHECK_THROWS_AS(Report::from_json(bad_duration), std::invalid_argument);

    auto bad_duration_type = j;
    bad_duration_type["actual_duration_sec"] = "long";
    CHECK_THROWS_AS(Report::from_json(bad_duration_type), std::invalid_argument);

    auto stop_before_start = j;
    stop_before_start["actual_stop_sec"] = 9.0;
    CHECK_THROWS_AS(Report::from_json(stop_before_start), std::invalid_argument);

    auto mismatched_duration = j;
    mismatched_duration["actual_duration_sec"] = 1.0;
    CHECK_THROWS_AS(Report::from_json(mismatched_duration), std::invalid_argument);

    auto bad_marker = j;
    bad_marker["marker_events"][0]["wall_clock_sec"] = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS_AS(Report::from_json(bad_marker), std::invalid_argument);

    auto bad_marker_name = j;
    bad_marker_name["marker_events"][0]["name"] = 42;
    CHECK_THROWS_AS(Report::from_json(bad_marker_name), std::invalid_argument);
}

TEST_CASE("Report rejects invalid device channel fields", "[reporting][report]") {
    Report r;
    r.scenario_name = "test_scenario";
    r.status = "completed";
    r.devices = {{"usrp0", "B210", 0}};
    auto j = r.to_json();

    auto negative_channel = j;
    negative_channel["devices"][0]["channel"] = -1;
    CHECK_THROWS_AS(Report::from_json(negative_channel), std::invalid_argument);

    auto fractional_channel = j;
    fractional_channel["devices"][0]["channel"] = 1.5;
    CHECK_THROWS_AS(Report::from_json(fractional_channel), std::invalid_argument);

    auto huge_channel = j;
    huge_channel["devices"][0]["channel"] = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1u;
    CHECK_THROWS_AS(Report::from_json(huge_channel), std::invalid_argument);
}

TEST_CASE("Report rejects invalid string fields", "[reporting][report]") {
    Report r;
    r.scenario_name = "test_scenario";
    r.status = "completed";
    r.devices = {{"usrp0", "B210", 0}};
    r.artifact_paths = {"/tmp/run/scenario.json"};
    auto j = r.to_json();

    auto bad_status = j;
    bad_status["status"] = false;
    CHECK_THROWS_AS(Report::from_json(bad_status), std::invalid_argument);

    auto bad_device_id = j;
    bad_device_id["devices"][0]["device_id"] = 42;
    CHECK_THROWS_AS(Report::from_json(bad_device_id), std::invalid_argument);

    auto bad_artifact_paths = j;
    bad_artifact_paths["artifact_paths"][0] = 42;
    CHECK_THROWS_AS(Report::from_json(bad_artifact_paths), std::invalid_argument);
}

TEST_CASE("Report rejects invalid container fields", "[reporting][report]") {
    Report r;
    r.scenario_name = "test_scenario";
    r.scenario_hash = "abc123";
    r.status = "completed";
    r.devices = {{"usrp0", "B210", 0}};
    r.warnings = {{ErrorCategory::QualityWarning, "W001", "low power"}};
    r.errors = {{ErrorCategory::Execution, "E001", "timeout"}};
    r.artifact_paths = {"/tmp/run/scenario.json"};
    r.marker_events.push_back({"m1", 1.0, 2.0});
    auto j = r.to_json();

    auto bad_devices = j;
    bad_devices["devices"] = nlohmann::json::object();
    CHECK_THROWS_AS(Report::from_json(bad_devices), std::invalid_argument);

    auto bad_warnings = j;
    bad_warnings["warnings"] = nlohmann::json::object();
    CHECK_THROWS_AS(Report::from_json(bad_warnings), std::invalid_argument);

    auto bad_errors = j;
    bad_errors["errors"] = nlohmann::json::object();
    CHECK_THROWS_AS(Report::from_json(bad_errors), std::invalid_argument);

    auto bad_artifact_paths = j;
    bad_artifact_paths["artifact_paths"] = nlohmann::json::object();
    CHECK_THROWS_AS(Report::from_json(bad_artifact_paths), std::invalid_argument);

    auto bad_marker_events = j;
    bad_marker_events["marker_events"] = nlohmann::json::object();
    CHECK_THROWS_AS(Report::from_json(bad_marker_events), std::invalid_argument);

    auto bad_warning_entry = j;
    bad_warning_entry["warnings"][0] = "low power";
    CHECK_THROWS_AS(Report::from_json(bad_warning_entry), std::invalid_argument);

    auto bad_error_entry = j;
    bad_error_entry["errors"][0] = "timeout";
    CHECK_THROWS_AS(Report::from_json(bad_error_entry), std::invalid_argument);

    auto bad_warning_category_type = j;
    bad_warning_category_type["warnings"][0]["category"] = 42;
    CHECK_THROWS_AS(Report::from_json(bad_warning_category_type), std::invalid_argument);

    auto bad_error_code_type = j;
    bad_error_code_type["errors"][0]["code"] = false;
    CHECK_THROWS_AS(Report::from_json(bad_error_code_type), std::invalid_argument);

    auto empty_error_code = j;
    empty_error_code["errors"][0]["code"] = "";
    CHECK_THROWS_AS(Report::from_json(empty_error_code), std::invalid_argument);

    auto bad_warning_message_type = j;
    bad_warning_message_type["warnings"][0]["message"] = nlohmann::json::array();
    CHECK_THROWS_AS(Report::from_json(bad_warning_message_type), std::invalid_argument);

    auto empty_warning_message = j;
    empty_warning_message["warnings"][0]["message"] = "";
    CHECK_THROWS_AS(Report::from_json(empty_warning_message), std::invalid_argument);

    auto unknown_warning_category = j;
    unknown_warning_category["warnings"][0]["category"] = "Bogus";
    CHECK_THROWS_AS(Report::from_json(unknown_warning_category), std::invalid_argument);

    auto missing_warning_message = j;
    missing_warning_message["warnings"][0].erase("message");
    CHECK_THROWS_AS(Report::from_json(missing_warning_message), std::invalid_argument);

    auto bad_device_entry = j;
    bad_device_entry["devices"][0] = "usrp0";
    CHECK_THROWS_AS(Report::from_json(bad_device_entry), std::invalid_argument);

    auto bad_marker_entry = j;
    bad_marker_entry["marker_events"][0] = "m1";
    CHECK_THROWS_AS(Report::from_json(bad_marker_entry), std::invalid_argument);

    CHECK_THROWS_AS(Report::from_json(nlohmann::json::array()), std::invalid_argument);
}
