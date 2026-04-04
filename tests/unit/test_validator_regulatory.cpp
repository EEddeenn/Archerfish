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

TEST_CASE("Validator warns on GPS L1 band frequency", "[validator][regulatory]") {
    Scenario s;
    s.metadata.name = "gps_test";
    s.devices.push_back({"usrp0", 0, {1575.42e6, 10000000.0, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_warning(result, "W_REGULATED_BAND"));
}

TEST_CASE("Validator warns on aviation VHF frequency", "[validator][regulatory]") {
    Scenario s;
    s.metadata.name = "vhf_test";
    s.devices.push_back({"usrp0", 0, {120e6, 10000000.0, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_warning(result, "W_REGULATED_BAND"));
}

TEST_CASE("Validator no regulatory warning for ISM band", "[validator][regulatory]") {
    Scenario s;
    s.metadata.name = "ism_test";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10000000.0, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(result.ok());
    REQUIRE_FALSE(has_warning(result, "W_REGULATED_BAND"));
}

TEST_CASE("Validator checks regulatory for multiple devices independently", "[validator][regulatory]") {
    Scenario s;
    s.metadata.name = "multi_reg";
    s.devices.push_back({"usrp0", std::nullopt, {2.45e9, 10000000.0, 20.0}});
    s.devices.push_back({"usrp1", std::nullopt, {1575.42e6, 10000000.0, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});
    s.emitters.push_back({"cw2", "usrp1", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_warning(result, "W_REGULATED_BAND"));
}
