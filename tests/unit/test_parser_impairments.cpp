#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/parser.hpp"

using namespace archerfish::scenario;
using Catch::Matchers::WithinAbs;

static std::string make_base_scenario() {
    return R"({
        "metadata": { "name": "imp_test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "usrp0", "start_after_sec": 0, "duration_sec": 1,
                       "waveform": {"type": "cw", "amplitude": 0.5}, "impairments": {IMPAIRMENT_PLACEHOLDER}}]
    })";
}

TEST_CASE("Parse CFO impairment", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("cfo_hz": 200.0)");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE(imp->cfo_hz.has_value());
    REQUIRE_THAT(*imp->cfo_hz, WithinAbs(200.0, 1e-12));
}

TEST_CASE("Parse phase_offset impairment", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("phase_offset_rad": 0.5)");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE(imp->phase_offset_rad.has_value());
    REQUIRE_THAT(*imp->phase_offset_rad, WithinAbs(0.5, 1e-12));
}

TEST_CASE("Parse IQ imbalance impairments", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("iq_gain_imbalance_db": 1.5, "iq_phase_imbalance_rad": 0.03)");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE_THAT(*imp->iq_gain_imbalance_db, WithinAbs(1.5, 1e-12));
    REQUIRE_THAT(*imp->iq_phase_imbalance_rad, WithinAbs(0.03, 1e-12));
}

TEST_CASE("Parse DC offset impairments", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("dc_offset_i": 0.01, "dc_offset_q": -0.02)");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE_THAT(*imp->dc_offset_i, WithinAbs(0.01, 1e-12));
    REQUIRE_THAT(*imp->dc_offset_q, WithinAbs(-0.02, 1e-12));
}

TEST_CASE("Parse AWGN impairment", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("awgn_power": 0.001)");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE_THAT(*imp->awgn_power, WithinAbs(0.001, 1e-12));
}

TEST_CASE("Parse amplitude ripple impairments", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("amplitude_ripple_db": 0.1, "amplitude_ripple_freq_hz": 1000.0)");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE_THAT(*imp->amplitude_ripple_db, WithinAbs(0.1, 1e-12));
    REQUIRE_THAT(*imp->amplitude_ripple_freq_hz, WithinAbs(1000.0, 1e-12));
}

TEST_CASE("Parse delay impairment", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("delay_sec": 0.0001)");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE_THAT(*imp->delay_sec, WithinAbs(0.0001, 1e-12));
}

TEST_CASE("Parse burst dropout impairment", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("burst_dropout_rate": 0.01)");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE_THAT(*imp->burst_dropout_rate, WithinAbs(0.01, 1e-12));
}

TEST_CASE("Parse phase noise impairment", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("phase_noise_bandwidth_hz": 100.0, "phase_noise_magnitude_rad": 0.1, "phase_noise_psd_shape": "1f")");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE_THAT(*imp->phase_noise_bandwidth_hz, WithinAbs(100.0, 1e-12));
    REQUIRE_THAT(*imp->phase_noise_magnitude_rad, WithinAbs(0.1, 1e-12));
    REQUIRE(imp->phase_noise_psd_shape.has_value());
    REQUIRE(*imp->phase_noise_psd_shape == "1f");
}

TEST_CASE("Parse multipath impairment", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("multipath_delay_samples": 5.0, "multipath_amplitude": 0.3)");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE_THAT(*imp->multipath_delay_samples, WithinAbs(5.0, 1e-12));
    REQUIRE_THAT(*imp->multipath_amplitude, WithinAbs(0.3, 1e-12));
}

TEST_CASE("Parse all impairments simultaneously", "[parser][impairments]") {
    std::string json_str = make_base_scenario();
    auto pos = json_str.find("IMPAIRMENT_PLACEHOLDER");
    json_str.replace(pos, strlen("IMPAIRMENT_PLACEHOLDER"),
        R"("cfo_hz": 100.0, "phase_offset_rad": 0.1, "iq_gain_imbalance_db": 0.5, "iq_phase_imbalance_rad": 0.02, "dc_offset_i": 0.01, "dc_offset_q": -0.01, "awgn_power": 0.001, "amplitude_ripple_db": 0.1, "amplitude_ripple_freq_hz": 1000.0, "delay_sec": 0.0001, "burst_dropout_rate": 0.01, "phase_noise_bandwidth_hz": 50.0, "phase_noise_magnitude_rad": 0.05, "multipath_delay_samples": 3.0, "multipath_amplitude": 0.2, "fading_doppler_hz": 10.0, "fading_type": "rayleigh", "pa_model": "ripple")");
    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& imp = result->emitters[0].impairments;
    REQUIRE(imp.has_value());
    REQUIRE(imp->cfo_hz.has_value());
    REQUIRE(imp->phase_offset_rad.has_value());
    REQUIRE(imp->iq_gain_imbalance_db.has_value());
    REQUIRE(imp->iq_phase_imbalance_rad.has_value());
    REQUIRE(imp->dc_offset_i.has_value());
    REQUIRE(imp->dc_offset_q.has_value());
    REQUIRE(imp->awgn_power.has_value());
    REQUIRE(imp->amplitude_ripple_db.has_value());
    REQUIRE(imp->amplitude_ripple_freq_hz.has_value());
    REQUIRE(imp->delay_sec.has_value());
    REQUIRE(imp->burst_dropout_rate.has_value());
    REQUIRE(imp->phase_noise_bandwidth_hz.has_value());
    REQUIRE(imp->phase_noise_magnitude_rad.has_value());
    REQUIRE(imp->multipath_delay_samples.has_value());
    REQUIRE(imp->multipath_amplitude.has_value());
    REQUIRE(imp->fading_doppler_hz.has_value());
    REQUIRE(imp->fading_type.has_value());
    REQUIRE(imp->pa_model.has_value());
}
