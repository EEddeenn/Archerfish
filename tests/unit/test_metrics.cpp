#include <catch2/catch_test_macros.hpp>

#include "archerfish/reporting/metrics.hpp"

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
