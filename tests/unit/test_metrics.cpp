#include <catch2/catch_test_macros.hpp>

#include "archerfish/reporting/metrics.hpp"

#include <limits>
#include <stdexcept>

using namespace archerfish::reporting;

TEST_CASE("Metrics default construction", "[reporting][metrics]") {
    Metrics m;
    CHECK(m.start_requested_sec == 0.0);
    CHECK(m.start_actual_sec == 0.0);
    CHECK(m.stop_actual_sec == 0.0);
    CHECK(m.tx_duration_sec == 0.0);
    CHECK(m.underrun_count == 0);
    CHECK(m.late_command_count == 0);
    CHECK(m.queue_depth_stats.avg_depth == 0.0);
    CHECK(m.queue_depth_stats.max_depth == 0.0);
    CHECK(m.queue_depth_stats.overflow_count == 0);
    CHECK(m.warning_count == 0);
    CHECK(m.error_count == 0);
    CHECK(m.total_samples_sent == 0);
    CHECK(m.scenario_hash.empty());
}

TEST_CASE("Metrics to_json contains expected keys", "[reporting][metrics]") {
    Metrics m;
    auto j = m.to_json();

    CHECK(j.contains("start_requested_sec"));
    CHECK(j.contains("start_actual_sec"));
    CHECK(j.contains("stop_actual_sec"));
    CHECK(j.contains("tx_duration_sec"));
    CHECK(j.contains("underrun_count"));
    CHECK(j.contains("late_command_count"));
    CHECK(j.contains("queue_depth_stats"));
    CHECK(j.contains("warning_count"));
    CHECK(j.contains("error_count"));
    CHECK(j.contains("total_samples_sent"));
    CHECK(j.contains("scenario_hash"));

    CHECK(j["queue_depth_stats"].contains("avg_depth"));
    CHECK(j["queue_depth_stats"].contains("max_depth"));
    CHECK(j["queue_depth_stats"].contains("overflow_count"));
}

TEST_CASE("Metrics round-trip preserves all fields", "[reporting][metrics]") {
    Metrics original;
    original.start_requested_sec = 1000.0;
    original.start_actual_sec = 1001.5;
    original.stop_actual_sec = 1010.25;
    original.tx_duration_sec = 8.75;
    original.underrun_count = 3;
    original.late_command_count = 7;
    original.queue_depth_stats = {4.2, 15.0, 2};
    original.warning_count = 5;
    original.error_count = 1;
    original.total_samples_sent = 1000000;
    original.scenario_hash = "abc123def456";

    auto j = original.to_json();
    auto restored = Metrics::from_json(j);

    CHECK(restored.start_requested_sec == original.start_requested_sec);
    CHECK(restored.start_actual_sec == original.start_actual_sec);
    CHECK(restored.stop_actual_sec == original.stop_actual_sec);
    CHECK(restored.tx_duration_sec == original.tx_duration_sec);
    CHECK(restored.underrun_count == original.underrun_count);
    CHECK(restored.late_command_count == original.late_command_count);
    CHECK(restored.queue_depth_stats.avg_depth == original.queue_depth_stats.avg_depth);
    CHECK(restored.queue_depth_stats.max_depth == original.queue_depth_stats.max_depth);
    CHECK(restored.queue_depth_stats.overflow_count == original.queue_depth_stats.overflow_count);
    CHECK(restored.warning_count == original.warning_count);
    CHECK(restored.error_count == original.error_count);
    CHECK(restored.total_samples_sent == original.total_samples_sent);
    CHECK(restored.scenario_hash == original.scenario_hash);
}

TEST_CASE("Metrics rejects invalid numeric fields", "[reporting][metrics]") {
    Metrics original;
    original.queue_depth_stats = {1.0, 2.0, 0};
    original.scenario_hash = "abc";
    auto j = original.to_json();

    auto negative_count = j;
    negative_count["underrun_count"] = -1;
    CHECK_THROWS_AS(Metrics::from_json(negative_count), std::invalid_argument);

    auto fractional_count = j;
    fractional_count["underrun_count"] = 1.5;
    CHECK_THROWS_AS(Metrics::from_json(fractional_count), std::invalid_argument);

    auto bad_time = j;
    bad_time["tx_duration_sec"] = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS_AS(Metrics::from_json(bad_time), std::invalid_argument);

    auto bad_time_type = j;
    bad_time_type["tx_duration_sec"] = "soon";
    CHECK_THROWS_AS(Metrics::from_json(bad_time_type), std::invalid_argument);

    auto bad_queue = j;
    bad_queue["queue_depth_stats"]["avg_depth"] = -1.0;
    CHECK_THROWS_AS(Metrics::from_json(bad_queue), std::invalid_argument);

    auto impossible_queue = j;
    impossible_queue["queue_depth_stats"]["avg_depth"] = 3.0;
    impossible_queue["queue_depth_stats"]["max_depth"] = 2.0;
    CHECK_THROWS_AS(Metrics::from_json(impossible_queue), std::invalid_argument);

    auto bad_hash = j;
    bad_hash["scenario_hash"] = 42;
    CHECK_THROWS_AS(Metrics::from_json(bad_hash), std::invalid_argument);
}

TEST_CASE("Metrics rejects inconsistent timing windows", "[reporting][metrics]") {
    Metrics original;
    original.start_actual_sec = 10.0;
    original.stop_actual_sec = 12.5;
    original.tx_duration_sec = 2.5;
    original.queue_depth_stats = {1.0, 2.0, 0};
    original.scenario_hash = "abc";
    auto j = original.to_json();

    auto stop_before_start = j;
    stop_before_start["stop_actual_sec"] = 9.0;
    CHECK_THROWS_AS(Metrics::from_json(stop_before_start), std::invalid_argument);

    auto mismatched_duration = j;
    mismatched_duration["tx_duration_sec"] = 1.0;
    CHECK_THROWS_AS(Metrics::from_json(mismatched_duration), std::invalid_argument);
}

TEST_CASE("Metrics rejects invalid object containers", "[reporting][metrics]") {
    Metrics original;
    original.queue_depth_stats = {1.0, 2.0, 0};
    original.scenario_hash = "abc";
    auto j = original.to_json();

    CHECK_THROWS_AS(Metrics::from_json(nlohmann::json::array()), std::invalid_argument);

    auto bad_queue_container = j;
    bad_queue_container["queue_depth_stats"] = "none";
    CHECK_THROWS_AS(Metrics::from_json(bad_queue_container), std::invalid_argument);
}
