#include <catch2/catch_test_macros.hpp>

#include "archerfish/reporting/emitter_metrics.hpp"

#include <limits>
#include <stdexcept>

using namespace archerfish::reporting;

TEST_CASE("EmitterMetrics default construction", "[reporting][emitter_metrics]") {
    EmitterMetrics m;
    CHECK(m.emitter_id.empty());
    CHECK(m.device_id.empty());
    CHECK(m.start_time_sec == 0.0);
    CHECK(m.duration_sec == 0.0);
    CHECK(m.samples_rendered == 0);
    CHECK(m.peak_amplitude == 0.0);
    CHECK(m.rms_amplitude == 0.0);
    CHECK(m.crest_factor == 0.0);
    CHECK(m.nominal_bandwidth == 0.0);
    CHECK(m.waveform_type.empty());
    CHECK(m.completed == false);
}

TEST_CASE("EmitterMetrics to_json contains expected keys", "[reporting][emitter_metrics]") {
    EmitterMetrics m;
    m.emitter_id = "cw1";
    m.device_id = "usrp0";
    auto j = to_json(m);

    CHECK(j.contains("emitter_id"));
    CHECK(j.contains("device_id"));
    CHECK(j.contains("start_time_sec"));
    CHECK(j.contains("duration_sec"));
    CHECK(j.contains("samples_rendered"));
    CHECK(j.contains("peak_amplitude"));
    CHECK(j.contains("rms_amplitude"));
    CHECK(j.contains("crest_factor"));
    CHECK(j.contains("nominal_bandwidth"));
    CHECK(j.contains("waveform_type"));
    CHECK(j.contains("completed"));
}

TEST_CASE("EmitterMetrics to_json round-trip via ScenarioMetricsSummary", "[reporting][emitter_metrics]") {
    EmitterMetrics m1;
    m1.emitter_id = "cw1";
    m1.device_id = "usrp0";
    m1.start_time_sec = 0.5;
    m1.duration_sec = 1.0;
    m1.samples_rendered = 10000000;
    m1.peak_amplitude = 0.9;
    m1.rms_amplitude = 0.5;
    m1.crest_factor = 1.8;
    m1.nominal_bandwidth = 1e6;
    m1.waveform_type = "cw";
    m1.completed = true;

    EmitterMetrics m2;
    m2.emitter_id = "chirp1";
    m2.device_id = "usrp0";
    m2.start_time_sec = 1.5;
    m2.duration_sec = 0.5;
    m2.samples_rendered = 5000000;
    m2.peak_amplitude = 0.3;
    m2.rms_amplitude = 0.2;
    m2.crest_factor = 1.5;
    m2.nominal_bandwidth = 2e6;
    m2.waveform_type = "chirp";
    m2.completed = true;

    ScenarioMetricsSummary original;
    original.total_emitters = 2;
    original.completed_emitters = 2;
    original.total_duration_sec = 2.0;
    original.scenario_start_sec = 0.5;
    original.scenario_end_sec = 2.0;
    original.emitters["cw1"] = m1;
    original.emitters["chirp1"] = m2;

    auto j = to_json(original);
    auto restored = from_json(j);

    CHECK(restored.total_emitters == original.total_emitters);
    CHECK(restored.completed_emitters == original.completed_emitters);
    CHECK(restored.total_duration_sec == original.total_duration_sec);
    CHECK(restored.scenario_start_sec == original.scenario_start_sec);
    CHECK(restored.scenario_end_sec == original.scenario_end_sec);
    REQUIRE(restored.emitters.size() == 2);

    const auto& r1 = restored.emitters.at("cw1");
    CHECK(r1.emitter_id == "cw1");
    CHECK(r1.device_id == "usrp0");
    CHECK(r1.start_time_sec == 0.5);
    CHECK(r1.duration_sec == 1.0);
    CHECK(r1.samples_rendered == 10000000);
    CHECK(r1.peak_amplitude == 0.9);
    CHECK(r1.rms_amplitude == 0.5);
    CHECK(r1.crest_factor == 1.8);
    CHECK(r1.nominal_bandwidth == 1e6);
    CHECK(r1.waveform_type == "cw");
    CHECK(r1.completed == true);

    const auto& r2 = restored.emitters.at("chirp1");
    CHECK(r2.emitter_id == "chirp1");
    CHECK(r2.device_id == "usrp0");
    CHECK(r2.start_time_sec == 1.5);
    CHECK(r2.duration_sec == 0.5);
    CHECK(r2.waveform_type == "chirp");
    CHECK(r2.completed == true);
}

TEST_CASE("ScenarioMetricsSummary to_json structure", "[reporting][emitter_metrics]") {
    ScenarioMetricsSummary s;
    s.total_emitters = 3;
    s.completed_emitters = 2;
    s.total_duration_sec = 5.0;
    s.scenario_start_sec = 0.0;
    s.scenario_end_sec = 5.0;

    auto j = to_json(s);

    CHECK(j.contains("total_emitters"));
    CHECK(j.contains("completed_emitters"));
    CHECK(j.contains("total_duration_sec"));
    CHECK(j.contains("scenario_start_sec"));
    CHECK(j.contains("scenario_end_sec"));
    CHECK(j.contains("emitters"));
    CHECK(j["emitters"].is_object());
}

TEST_CASE("ScenarioMetricsSummary empty round-trip", "[reporting][emitter_metrics]") {
    ScenarioMetricsSummary original;
    auto j = to_json(original);
    auto restored = from_json(j);

    CHECK(restored.total_emitters == 0);
    CHECK(restored.completed_emitters == 0);
    CHECK(restored.total_duration_sec == 0.0);
    CHECK(restored.scenario_start_sec == 0.0);
    CHECK(restored.scenario_end_sec == 0.0);
    CHECK(restored.emitters.empty());
}

TEST_CASE("ScenarioMetricsSummary rejects invalid numeric fields", "[reporting][emitter_metrics]") {
    EmitterMetrics m;
    m.emitter_id = "cw1";
    m.device_id = "usrp0";
    m.samples_rendered = 10;
    m.completed = true;

    ScenarioMetricsSummary summary;
    summary.total_emitters = 1;
    summary.completed_emitters = 1;
    summary.emitters["cw1"] = m;

    auto j = to_json(summary);

    auto bad_total = j;
    bad_total["total_emitters"] = -1;
    CHECK_THROWS_AS(from_json(bad_total), std::invalid_argument);

    auto fractional_total = j;
    fractional_total["total_emitters"] = 1.5;
    CHECK_THROWS_AS(from_json(fractional_total), std::invalid_argument);

    auto bad_samples = j;
    bad_samples["emitters"]["cw1"]["samples_rendered"] = -1;
    CHECK_THROWS_AS(from_json(bad_samples), std::invalid_argument);

    auto fractional_samples = j;
    fractional_samples["emitters"]["cw1"]["samples_rendered"] = 1.5;
    CHECK_THROWS_AS(from_json(fractional_samples), std::invalid_argument);

    auto bad_peak = j;
    bad_peak["emitters"]["cw1"]["peak_amplitude"] = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS_AS(from_json(bad_peak), std::invalid_argument);

    auto bad_peak_type = j;
    bad_peak_type["emitters"]["cw1"]["peak_amplitude"] = "loud";
    CHECK_THROWS_AS(from_json(bad_peak_type), std::invalid_argument);

    auto bad_emitter_id = j;
    bad_emitter_id["emitters"]["cw1"]["emitter_id"] = 42;
    CHECK_THROWS_AS(from_json(bad_emitter_id), std::invalid_argument);

    auto mismatched_emitter_id = j;
    mismatched_emitter_id["emitters"]["cw1"]["emitter_id"] = "other";
    CHECK_THROWS_AS(from_json(mismatched_emitter_id), std::invalid_argument);

    auto empty_emitter_id = j;
    empty_emitter_id["emitters"]["cw1"]["emitter_id"] = "";
    CHECK_THROWS_AS(from_json(empty_emitter_id), std::invalid_argument);

    auto blank_emitter_id = j;
    blank_emitter_id["emitters"]["cw1"]["emitter_id"] = " \t\n";
    CHECK_THROWS_AS(from_json(blank_emitter_id), std::invalid_argument);

    auto bad_completed = j;
    bad_completed["emitters"]["cw1"]["completed"] = "true";
    CHECK_THROWS_AS(from_json(bad_completed), std::invalid_argument);
}

TEST_CASE("ScenarioMetricsSummary rejects invalid emitter containers", "[reporting][emitter_metrics]") {
    EmitterMetrics m;
    m.emitter_id = "cw1";
    m.device_id = "usrp0";
    m.completed = true;

    ScenarioMetricsSummary summary;
    summary.total_emitters = 1;
    summary.completed_emitters = 1;
    summary.emitters["cw1"] = m;

    auto j = to_json(summary);

    auto emitters_array = j;
    emitters_array["emitters"] = nlohmann::json::array({j["emitters"]["cw1"]});
    CHECK_THROWS_AS(from_json(emitters_array), std::invalid_argument);

    auto emitters_string = j;
    emitters_string["emitters"] = "cw1";
    CHECK_THROWS_AS(from_json(emitters_string), std::invalid_argument);

    auto scalar_entry = j;
    scalar_entry["emitters"]["cw1"] = "complete";
    CHECK_THROWS_AS(from_json(scalar_entry), std::invalid_argument);

    auto empty_key = j;
    empty_key["emitters"].erase("cw1");
    empty_key["emitters"][""] = j["emitters"]["cw1"];
    empty_key["emitters"][""]["emitter_id"] = "";
    CHECK_THROWS_AS(from_json(empty_key), std::invalid_argument);

    CHECK_THROWS_AS(from_json(nlohmann::json::array()), std::invalid_argument);
}

TEST_CASE("ScenarioMetricsSummary rejects inconsistent aggregate counts", "[reporting][emitter_metrics]") {
    EmitterMetrics m;
    m.emitter_id = "cw1";
    m.device_id = "usrp0";
    m.completed = true;

    ScenarioMetricsSummary summary;
    summary.total_emitters = 1;
    summary.completed_emitters = 1;
    summary.emitters["cw1"] = m;

    auto j = to_json(summary);

    auto bad_total = j;
    bad_total["total_emitters"] = 2;
    CHECK_THROWS_AS(from_json(bad_total), std::invalid_argument);

    auto bad_completed = j;
    bad_completed["completed_emitters"] = 0;
    CHECK_THROWS_AS(from_json(bad_completed), std::invalid_argument);

    auto bad_time_window = j;
    bad_time_window["scenario_start_sec"] = 5.0;
    bad_time_window["scenario_end_sec"] = 4.0;
    CHECK_THROWS_AS(from_json(bad_time_window), std::invalid_argument);
}
