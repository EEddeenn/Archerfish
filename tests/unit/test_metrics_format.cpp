#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/reporting/metrics.hpp"

using namespace archerfish::reporting;
using Catch::Matchers::WithinAbs;

TEST_CASE("Metrics to_json has correct types for numeric fields", "[reporting][metrics]") {
    Metrics m;
    m.start_requested_sec = 1000.0;
    m.underrun_count = 3;
    m.total_samples_sent = 500000;

    auto j = m.to_json();
    CHECK(j["start_requested_sec"].is_number());
    CHECK(j["underrun_count"].is_number());
    CHECK(j["total_samples_sent"].is_number());
    CHECK(j["scenario_hash"].is_string());
}

TEST_CASE("Metrics JSON queue_depth_stats is nested object", "[reporting][metrics]") {
    Metrics m;
    m.queue_depth_stats = {3.5, 20.0, 1};

    auto j = m.to_json();
    REQUIRE(j.contains("queue_depth_stats"));
    REQUIRE(j["queue_depth_stats"].is_object());
    CHECK(j["queue_depth_stats"]["avg_depth"].get<double>() == 3.5);
    CHECK(j["queue_depth_stats"]["max_depth"].get<double>() == 20.0);
    CHECK(j["queue_depth_stats"]["overflow_count"].get<size_t>() == 1);
}

TEST_CASE("Metrics JSON preserves large sample counts", "[reporting][metrics]") {
    Metrics m;
    m.total_samples_sent = 10000000000ULL;

    auto j = m.to_json();
    auto restored = Metrics::from_json(j);
    CHECK(restored.total_samples_sent == 10000000000ULL);
}

TEST_CASE("Metrics from_json handles zero values", "[reporting][metrics]") {
    auto j = R"({
        "start_requested_sec": 0.0,
        "start_actual_sec": 0.0,
        "stop_actual_sec": 0.0,
        "tx_duration_sec": 0.0,
        "underrun_count": 0,
        "late_command_count": 0,
        "queue_depth_stats": {"avg_depth": 0.0, "max_depth": 0.0, "overflow_count": 0},
        "warning_count": 0,
        "error_count": 0,
        "total_samples_sent": 0,
        "scenario_hash": ""
    })"_json;

    auto m = Metrics::from_json(j);
    CHECK(m.start_requested_sec == 0.0);
    CHECK(m.underrun_count == 0);
    CHECK(m.total_samples_sent == 0);
}

TEST_CASE("Metrics roundtrip with all fields populated", "[reporting][metrics]") {
    Metrics m;
    m.start_requested_sec = 1234.5;
    m.start_actual_sec = 1234.6;
    m.stop_actual_sec = 1244.5;
    m.tx_duration_sec = 9.9;
    m.underrun_count = 2;
    m.late_command_count = 5;
    m.queue_depth_stats = {6.7, 32.0, 3};
    m.warning_count = 4;
    m.error_count = 1;
    m.total_samples_sent = 99000000;
    m.scenario_hash = "sha256:abcdef1234567890";

    auto j = m.to_json();
    auto r = Metrics::from_json(j);
    CHECK_THAT(r.start_requested_sec, WithinAbs(1234.5, 1e-12));
    CHECK_THAT(r.tx_duration_sec, WithinAbs(9.9, 1e-12));
    CHECK(r.underrun_count == 2);
    CHECK(r.late_command_count == 5);
    CHECK_THAT(r.queue_depth_stats.avg_depth, WithinAbs(6.7, 1e-12));
    CHECK(r.total_samples_sent == 99000000);
    CHECK(r.scenario_hash == "sha256:abcdef1234567890");
}
