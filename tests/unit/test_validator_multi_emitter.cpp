#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/validator.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;

static bool has_error(const ValidationResult& r, const std::string& code) {
    return std::any_of(r.errors.begin(), r.errors.end(),
                       [&](const Error& e) { return e.code == code; });
}

static bool has_warning(const ValidationResult& r, const std::string& code) {
    return std::any_of(r.warnings.begin(), r.warnings.end(),
                       [&](const Error& e) { return e.code == code; });
}

TEST_CASE("Three emitters with partial overlap on same channel produces error", "[validator][multi_emitter]") {
    Scenario s;
    s.metadata.name = "multi_overlap";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
    s.emitters.push_back({"e1", "usrp0", 0, 0.0, 3.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});
    s.emitters.push_back({"e2", "usrp0", 0, 1.0, 2.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});
    s.emitters.push_back({"e3", "usrp0", 0, 2.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_error(result, "V002_OVERLAPPING_EMITTERS"));
}

TEST_CASE("Additive mixing with headroom warning", "[validator][multi_emitter]") {
    Scenario s;
    s.metadata.name = "additive_headroom";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
    s.emitters.push_back({"e1", "usrp0", 0, 0.0, 2.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.6}}},
                          std::nullopt, std::nullopt, MixingMode::Additive, std::nullopt, std::nullopt});
    s.emitters.push_back({"e2", "usrp0", 0, 0.0, 2.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.5}}},
                          std::nullopt, std::nullopt, MixingMode::Additive, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_warning(result, "W_MIX_HEADROOM"));
}

TEST_CASE("Additive mixing within headroom passes", "[validator][multi_emitter]") {
    Scenario s;
    s.metadata.name = "additive_ok";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
    s.emitters.push_back({"e1", "usrp0", 0, 0.0, 2.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::Additive, std::nullopt, std::nullopt});
    s.emitters.push_back({"e2", "usrp0", 0, 0.0, 2.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::Additive, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(result.ok());
    REQUIRE_FALSE(has_warning(result, "W_MIX_HEADROOM"));
}

TEST_CASE("Overlapping emitters with one additive one not produces error", "[validator][multi_emitter]") {
    Scenario s;
    s.metadata.name = "mixed_mixing";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
    s.emitters.push_back({"e1", "usrp0", 0, 0.0, 2.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::Additive, std::nullopt, std::nullopt});
    s.emitters.push_back({"e2", "usrp0", 0, 0.0, 2.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(has_error(result, "V002_OVERLAPPING_EMITTERS"));
}

TEST_CASE("Multiple non-overlapping emitters on same channel passes", "[validator][multi_emitter]") {
    Scenario s;
    s.metadata.name = "multi_sequential";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
    s.emitters.push_back({"e1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});
    s.emitters.push_back({"e2", "usrp0", 0, 1.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});
    s.emitters.push_back({"e3", "usrp0", 0, 2.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.4}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = validate(s);
    REQUIRE(result.ok());
}
