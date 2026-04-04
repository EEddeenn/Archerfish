#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/validator.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;

static bool has_warning(const ValidationResult& r, const std::string& code) {
    return std::any_of(r.warnings.begin(), r.warnings.end(),
                       [&](const Error& e) { return e.code == code; });
}

TEST_CASE("Validator safety warning for excessive gain", "[validator][safety]") {
    Scenario s;
    s.metadata.name = "safety_gain";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10000000.0, 25.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_warning(result, "W_SAFETY_GAIN_EXCEEDED"));
}

TEST_CASE("Validator safety warning for excessive amplitude", "[validator][safety]") {
    Scenario s;
    s.metadata.name = "safety_amp";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10000000.0, 10.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.8}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_warning(result, "W_SAFETY_AMPLITUDE_EXCEEDED"));
}

TEST_CASE("Validator safety warning for out-of-range frequency", "[validator][safety]") {
    Scenario s;
    s.metadata.name = "safety_freq";
    s.devices.push_back({"usrp0", 0, {100e6, 10000000.0, 10.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_warning(result, "W_SAFETY_FREQ_OUT_OF_RANGE"));
}

TEST_CASE("Validator no safety warning for safe parameters", "[validator][safety]") {
    Scenario s;
    s.metadata.name = "safety_ok";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10000000.0, 10.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE_FALSE(has_warning(result, "W_SAFETY_GAIN_EXCEEDED"));
    REQUIRE_FALSE(has_warning(result, "W_SAFETY_AMPLITUDE_EXCEEDED"));
    REQUIRE_FALSE(has_warning(result, "W_SAFETY_FREQ_OUT_OF_RANGE"));
}

TEST_CASE("Validator checks safety for multiple devices", "[validator][safety]") {
    Scenario s;
    s.metadata.name = "safety_multi";
    s.devices.push_back({"usrp0", std::nullopt, {2.45e9, 10000000.0, 10.0}});
    s.devices.push_back({"usrp1", std::nullopt, {2.45e9, 10000000.0, 30.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});
    s.emitters.push_back({"cw2", "usrp1", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_warning(result, "W_SAFETY_GAIN_EXCEEDED"));
}
