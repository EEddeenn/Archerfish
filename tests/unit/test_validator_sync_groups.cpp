#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/validator.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;

static Scenario make_scenario_with_channels() {
    Scenario s;
    s.metadata.name = "sync_test";
    s.devices.push_back({"usrp0", std::nullopt, {2450000000.0, 10000000.0, 20.0}});
    s.channel_defs.push_back({"ch0", "usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.channel_defs.push_back({"ch1", "usrp0", 1, {2450000000.0, 10000000.0, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, "ch0"});
    return s;
}

static bool has_error(const ValidationResult& r, const std::string& code) {
    return std::any_of(r.errors.begin(), r.errors.end(),
                       [&](const Error& e) { return e.code == code; });
}

TEST_CASE("Sync group with unknown channel produces error", "[validator][sync_groups]") {
    auto s = make_scenario_with_channels();
    s.sync_groups.push_back({"sg1", {"ch0", "nonexistent"}, "coherent"});

    auto result = validate(s);
    REQUIRE(has_error(result, "V024_SYNC_UNKNOWN_CHANNEL"));
}

TEST_CASE("Coherent group with different devices produces error", "[validator][sync_groups]") {
    Scenario s;
    s.metadata.name = "sync_diff_dev";
    s.devices.push_back({"usrp0", std::nullopt, {2450000000.0, 10000000.0, 20.0}});
    s.devices.push_back({"usrp1", std::nullopt, {2450000000.0, 10000000.0, 20.0}});
    s.channel_defs.push_back({"ch0", "usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.channel_defs.push_back({"ch1", "usrp1", 0, {2450000000.0, 10000000.0, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, "ch0"});
    s.sync_groups.push_back({"sg1", {"ch0", "ch1"}, "coherent"});

    auto result = validate(s);
    REQUIRE(has_error(result, "V025_COHERENT_DIFFERENT_DEVICES"));
}

TEST_CASE("Coherent group with different rates produces error", "[validator][sync_groups]") {
    Scenario s;
    s.metadata.name = "sync_diff_rate";
    s.devices.push_back({"usrp0", std::nullopt, {2450000000.0, 10000000.0, 20.0}});
    s.channel_defs.push_back({"ch0", "usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.channel_defs.push_back({"ch1", "usrp0", 1, {2450000000.0, 20000000.0, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 1.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, "ch0"});
    s.sync_groups.push_back({"sg1", {"ch0", "ch1"}, "coherent"});

    auto result = validate(s);
    REQUIRE(has_error(result, "V026_COHERENT_DIFFERENT_RATES"));
}

TEST_CASE("Valid coherent group passes validation", "[validator][sync_groups]") {
    auto s = make_scenario_with_channels();
    s.sync_groups.push_back({"sg1", {"ch0", "ch1"}, "coherent"});

    auto result = validate(s);
    REQUIRE(result.ok());
}

TEST_CASE("Independent mode sync group passes validation", "[validator][sync_groups]") {
    auto s = make_scenario_with_channels();
    s.sync_groups.push_back({"sg1", {"ch0", "ch1"}, "independent"});

    auto result = validate(s);
    REQUIRE(result.ok());
}
