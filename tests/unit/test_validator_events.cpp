#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/validator.hpp"
#include "archerfish/dsp/waveform_type.hpp"

#include <cstdint>
#include <limits>

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

TEST_CASE("Burst event type is rejected until it has execution semantics", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "burst", nlohmann::json::object()});

    auto result = validate(s);
    REQUIRE(has_error(result, "V014_INVALID_EVENT_TYPE"));
}

TEST_CASE("Retune event missing freq_hz produces error", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "retune", {{"other", 123}}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V015_RETUNE_MISSING_FREQ"));
}

TEST_CASE("Retune event invalid freq_hz produces error", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "retune", {{"freq_hz", std::numeric_limits<double>::quiet_NaN()}}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V015_RETUNE_INVALID_FREQ"));
}

TEST_CASE("Gain change event missing gain_db produces error", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "gain_change", {{"other", 123}}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V016_GAIN_MISSING_DB"));
}

TEST_CASE("Gain change event invalid gain_db produces error", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "gain_change", {{"gain_db", "high"}}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V016_GAIN_INVALID_DB"));
}

TEST_CASE("Non-finite or negative event time produces error", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", std::numeric_limits<double>::infinity(), "marker", {}});
    s.events.push_back({"usrp0", -1.0, "marker", {}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V014_INVALID_EVENT_TIME"));
}

TEST_CASE("Marker event rejects non-string name payload", "[validator][events]") {
    auto s = make_base_scenario();
    s.events.push_back({"usrp0", 1.0, "marker", {{"name", 123}}});

    auto result = validate(s);
    REQUIRE(has_error(result, "V021_MARKER_INVALID_NAME"));
}

TEST_CASE("Event string payloads must be non-empty", "[validator][events]") {
    SECTION("marker name") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "marker", {{"name", ""}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V021_MARKER_EMPTY_NAME"));
    }

    SECTION("waveform switch emitter") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "waveform_switch",
                            {{"emitter_id", ""}, {"new_waveform", "wf2"}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V019_WAVEFORM_SWITCH_EMPTY_EMITTER"));
    }

    SECTION("waveform switch waveform") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "waveform_switch",
                            {{"emitter_id", "cw1"}, {"new_waveform", ""}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V019_WAVEFORM_SWITCH_EMPTY_WAVEFORM"));
    }

    SECTION("impairment change emitter") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "impairment_change",
                            {{"emitter_id", ""}, {"impairment", "cfo_hz"}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V020_IMPAIRMENT_CHANGE_EMPTY_EMITTER"));
    }

    SECTION("impairment change name") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "impairment_change",
                            {{"emitter_id", "cw1"}, {"impairment", ""}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V020_IMPAIRMENT_CHANGE_EMPTY_IMPAIRMENT"));
    }
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

TEST_CASE("Retune and gain_change events reject invalid channel payloads", "[validator][events]") {
    SECTION("retune negative channel") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "retune", {{"freq_hz", 2400000000.0}, {"channel", -1}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V015_RETUNE_INVALID_CHANNEL"));
    }

    SECTION("retune fractional channel") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "retune", {{"freq_hz", 2400000000.0}, {"channel", 1.5}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V015_RETUNE_INVALID_CHANNEL"));
    }

    SECTION("gain_change out-of-range channel") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "gain_change",
                            {{"gain_db", 25.0},
                             {"channel", static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1u}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V016_GAIN_INVALID_CHANNEL"));
    }

    SECTION("gain_change string channel") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "gain_change", {{"gain_db", 25.0}, {"channel", "zero"}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V016_GAIN_INVALID_CHANNEL"));
    }
}

TEST_CASE("Retune and gain_change events reject unbound target channels", "[validator][events]") {
    SECTION("retune channel not bound to device") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "retune", {{"freq_hz", 2400000000.0}, {"channel", 7}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V015_RETUNE_UNKNOWN_CHANNEL"));
    }

    SECTION("gain channel not bound to device") {
        auto s = make_base_scenario();
        s.events.push_back({"usrp0", 1.0, "gain_change", {{"gain_db", 25.0}, {"channel", 7}}});

        auto result = validate(s);
        REQUIRE(has_error(result, "V016_GAIN_UNKNOWN_CHANNEL"));
    }
}

TEST_CASE("RF events accept channels declared through channel_defs", "[validator][events][channels]") {
    auto s = make_base_scenario();
    s.channel_defs.push_back({"ch1", "usrp0", 1, {2450000000.0, 10000000.0, 20.0}});
    s.emitters[0].channel = 1;
    s.emitters[0].channel_id = "ch1";
    s.events.push_back({"usrp0", 1.0, "gain_change", {{"gain_db", 25.0}, {"channel", 1}}});

    auto result = validate(s);
    REQUIRE_FALSE(has_error(result, "V016_GAIN_UNKNOWN_CHANNEL"));
}
