#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/validator.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;

static Scenario make_valid_scenario() {
    Scenario s;
    s.metadata.name = "test";
    s.devices.push_back({"usrp0", 0, {2450000000.0, 10000000.0, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 1.0, 4.0,
                          WaveformDef{std::nullopt, "cw", {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt});
    return s;
}

static bool has_error_with_code(const ValidationResult& r, const std::string& code) {
    return std::any_of(r.errors.begin(), r.errors.end(),
                       [&](const Error& e) { return e.code == code; });
}

static bool has_warning_with_code(const ValidationResult& r, const std::string& code) {
    return std::any_of(r.warnings.begin(), r.warnings.end(),
                       [&](const Error& e) { return e.code == code; });
}

TEST_CASE("Valid scenario returns ok()", "[validator]") {
    auto result = validate(make_valid_scenario());
    REQUIRE(result.ok());
    CHECK(result.errors.empty());
}

TEST_CASE("Amplitude > 1.0 produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].waveform->params["amplitude"] = 1.5;
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V001_INVALID_AMPLITUDE"));
}

TEST_CASE("Amplitude <= 0 produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].waveform->params["amplitude"] = -0.1;
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V001_INVALID_AMPLITUDE"));

    s.emitters[0].waveform->params["amplitude"] = 0.0;
    result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V001_INVALID_AMPLITUDE"));
}

TEST_CASE("Amplitude 0.95 produces clipping warning", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].waveform->params["amplitude"] = 0.95;
    auto result = validate(s);
    CHECK(result.ok());
    CHECK(has_warning_with_code(result, "V001_AMPLITUDE_CLIPPING_RISK"));
}

TEST_CASE("Duration <= 0 produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].duration_sec = 0.0;
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V002_INVALID_DURATION"));

    s.emitters[0].duration_sec = -1.0;
    result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V002_INVALID_DURATION"));
}

TEST_CASE("Dangling device reference produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].device = "nonexistent";
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V003_DANGLING_DEVICE_REF"));
}

TEST_CASE("Channel mismatch produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.devices[0].channel = 1;
    s.emitters[0].channel = 0;
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V004_CHANNEL_MISMATCH"));
}

TEST_CASE("Dangling waveform_ref produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].waveform = std::nullopt;
    s.emitters[0].waveform_ref = "missing_waveform";
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V005_DANGLING_WAVEFORM_REF"));
}

TEST_CASE("Emitter with no waveform and no waveform_ref produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].waveform = std::nullopt;
    s.emitters[0].waveform_ref = std::nullopt;
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V005_NO_WAVEFORM"));
}

TEST_CASE("Overlapping emitters on same channel produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters.push_back({"cw2", "usrp0", 0, 2.0, 3.0,
                          WaveformDef{std::nullopt, "cw", {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt});
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V002_OVERLAPPING_EMITTERS"));
}

TEST_CASE("Non-overlapping emitters on same channel is OK", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].start_after_sec = 0.0;
    s.emitters[0].duration_sec = 2.0;
    s.emitters.push_back({"cw2", "usrp0", 0, 2.0, 2.0,
                          WaveformDef{std::nullopt, "cw", {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt});
    auto result = validate(s);
    CHECK(result.ok());
}

TEST_CASE("Emitters on different channels don't conflict", "[validator]") {
    auto s = make_valid_scenario();
    s.devices[0].channel = std::nullopt;
    s.emitters.push_back({"cw2", "usrp0", 1, 1.0, 4.0,
                          WaveformDef{std::nullopt, "cw", {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt});
    auto result = validate(s);
    CHECK(result.ok());
}

TEST_CASE("Unknown waveform type produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].waveform->type = "unknown_type";
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V006_INVALID_WAVEFORM_TYPE"));
}

TEST_CASE("freq_hz <= 0 produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.devices[0].rf.freq_hz = 0.0;
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V007_INVALID_FREQ"));

    s.devices[0].rf.freq_hz = -1.0;
    result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V007_INVALID_FREQ"));
}

TEST_CASE("rate_sps <= 0 produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.devices[0].rf.rate_sps = 0.0;
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V007_INVALID_RATE"));
}

TEST_CASE("Empty devices produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.devices.clear();
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V008_NO_DEVICES"));
}

TEST_CASE("Empty emitters produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters.clear();
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V009_NO_EMITTERS"));
}

TEST_CASE("Duplicate device IDs produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.devices.push_back({"usrp0", 1, {3500000000.0, 20000000.0, 30.0}});
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V010_DUPLICATE_DEVICE_ID"));
}

TEST_CASE("Duplicate emitter IDs produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters.push_back({"cw1", "usrp0", 0, 10.0, 2.0,
                          WaveformDef{std::nullopt, "cw", {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt});
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V011_DUPLICATE_EMITTER_ID"));
}

TEST_CASE("Duplicate waveform IDs produces error", "[validator]") {
    auto s = make_valid_scenario();
    s.waveforms.push_back({"wf1", "cw", {{"amplitude", 0.2}}});
    s.waveforms.push_back({"wf1", "noise", {}});
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V012_DUPLICATE_WAVEFORM_ID"));
}

TEST_CASE("All validation errors use ErrorCategory::Validation", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].waveform->params["amplitude"] = 2.0;
    s.emitters[0].duration_sec = -1.0;
    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    for (const auto& e : result.errors) {
        CHECK(e.category == ErrorCategory::Validation);
    }
}

TEST_CASE("Warnings use ErrorCategory::QualityWarning", "[validator]") {
    auto s = make_valid_scenario();
    s.emitters[0].waveform->params["amplitude"] = 0.95;
    auto result = validate(s);
    CHECK(result.ok());
    for (const auto& w : result.warnings) {
        CHECK(w.category == ErrorCategory::QualityWarning);
    }
}

TEST_CASE("Emitter with valid waveform_ref resolves correctly", "[validator]") {
    auto s = make_valid_scenario();
    s.waveforms.push_back({"my_cw", "cw", {{"amplitude", 0.5}}});
    s.emitters[0].waveform = std::nullopt;
    s.emitters[0].waveform_ref = "my_cw";
    auto result = validate(s);
    CHECK(result.ok());
}

TEST_CASE("Valid waveform types are accepted", "[validator]") {
    auto s = make_valid_scenario();
    std::vector<std::string> types = {"cw", "chirp", "noise", "qpsk", "bpsk",
                                       "8psk", "qam16", "qam64", "multi_tone", "file"};
    for (const auto& t : types) {
        s.emitters[0].waveform->type = t;
        auto result = validate(s);
        CHECK(result.ok());
    }
}
