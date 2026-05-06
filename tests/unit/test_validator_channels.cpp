#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/validator.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;

static Scenario make_base_scenario() {
    Scenario s;
    s.metadata.name = "channel_test";
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

TEST_CASE("Duplicate channel IDs produces error", "[validator][channels]") {
    auto s = make_base_scenario();
    s.channel_defs.push_back({"ch0", "usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.channel_defs.push_back({"ch0", "usrp0", 1, {3500000000.0, 20000000.0, 20.0}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V020_DUPLICATE_CHANNEL_ID"));
}

TEST_CASE("Channel with unknown device produces error", "[validator][channels]") {
    auto s = make_base_scenario();
    s.channel_defs.push_back({"ch0", "nonexistent", 0, {2450000000.0, 10000000.0, 20.0}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V021_CHANNEL_UNKNOWN_DEVICE"));
}

TEST_CASE("Duplicate device+index pair produces error", "[validator][channels]") {
    auto s = make_base_scenario();
    s.channel_defs.push_back({"ch0", "usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.channel_defs.push_back({"ch1", "usrp0", 0, {3500000000.0, 20000000.0, 20.0}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V022_DUPLICATE_DEVICE_INDEX"));
}

TEST_CASE("Emitter with unknown channel_id produces error", "[validator][channels]") {
    auto s = make_base_scenario();
    s.channel_defs.push_back({"ch0", "usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.emitters[0].channel_id = "missing_channel";

    auto result = validate(s);
    REQUIRE(has_error(result, "V023_EMITTER_UNKNOWN_CHANNEL"));
}

TEST_CASE("Valid channel definitions pass validation", "[validator][channels]") {
    auto s = make_base_scenario();
    s.channel_defs.push_back({"ch0", "usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.emitters[0].channel_id = "ch0";

    auto result = validate(s);
    REQUIRE(result.ok());
}

TEST_CASE("Explicit channel RF settings must be valid", "[validator][channels]") {
    auto s = make_base_scenario();
    s.channel_defs.push_back({"ch0", "usrp0", 0, {0.0, -1.0, 20.0}});
    s.emitters[0].channel_id = "ch0";

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error(result, "V027_INVALID_CHANNEL_FREQ"));
    CHECK(has_error(result, "V027_INVALID_CHANNEL_RATE"));
}

TEST_CASE("Overlapping emitters on different explicit channels pass validation", "[validator][channels]") {
    auto s = make_base_scenario();
    s.channel_defs.push_back({"ch0", "usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.channel_defs.push_back({"ch1", "usrp0", 1, {2450000000.0, 10000000.0, 20.0}});
    s.emitters[0].channel_id = "ch0";

    auto second = s.emitters[0];
    second.id = "cw2";
    second.channel_id = "ch1";
    s.emitters.push_back(second);

    auto result = validate(s);
    REQUIRE(result.ok());
}

TEST_CASE("Overlapping emitters on same explicit channel fail validation", "[validator][channels]") {
    auto s = make_base_scenario();
    s.channel_defs.push_back({"ch0", "usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.emitters[0].channel_id = "ch0";

    auto second = s.emitters[0];
    second.id = "cw2";
    s.emitters.push_back(second);

    auto result = validate(s);
    REQUIRE(has_error(result, "V002_OVERLAPPING_EMITTERS"));
}
