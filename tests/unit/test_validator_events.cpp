#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/validator.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;

static Scenario make_base_scenario() {
    Scenario s;
    s.metadata.name = "event_test";
    s.devices.push_back({"usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});
    return s;
}

static bool has_error(const ValidationResult& r, const std::string& code) {
    return std::any_of(r.errors.begin(), r.errors.end(),
                       [&](const Error& e) { return e.code == code; });
}

TEST_CASE("Unknown event type produces error", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "unknown_type", {{"key", "val"}}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V014_INVALID_EVENT_TYPE"));
}

TEST_CASE("Retune event missing freq_hz produces error", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "retune", {{"other", 123}}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V015_RETUNE_MISSING_FREQ"));
}

TEST_CASE("Gain change event missing gain_db produces error", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "gain_change", {{"other", 123}}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V016_GAIN_MISSING_DB"));
}

TEST_CASE("Event with unknown target device produces error", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"nonexistent", 1.0, "retune", {{"freq_hz", 2.4e9}}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V013_EVENT_UNKNOWN_DEVICE"));
}

TEST_CASE("Valid retune event passes validation", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "retune", {{"freq_hz", 2400000000.0}}});

    auto result = validate(s);
    REQUIRE(result.ok());
}

TEST_CASE("Valid gain_change event passes validation", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "gain_change", {{"gain_db", 25.0}}});

    auto result = validate(s);
    REQUIRE(result.ok());
}
