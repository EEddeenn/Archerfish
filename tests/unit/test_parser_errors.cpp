#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "archerfish/scenario/parser.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("Parse completely empty string returns JSON parse error", "[parser][errors]") {
    auto result = parse_scenario_json("");
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Parse non-object JSON returns parse error", "[parser][errors]") {
    auto result = parse_scenario_json("[]");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Parse bare number returns parse error", "[parser][errors]") {
    auto result = parse_scenario_json("42");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Parse null returns parse error", "[parser][errors]") {
    auto result = parse_scenario_json("null");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Parse truncated JSON returns parse error", "[parser][errors]") {
    auto result = parse_scenario_json(R"({"metadata": {"name": "test")");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Device missing rf section returns error", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "no_rf" },
        "devices": [{ "id": "dev0" }],
        "emitters": [{ "id": "em0", "device": "dev0", "waveform": {"type": "cw"} }]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    bool found_rf = false;
    for (const auto& e : result.error()) {
        if (e.message.find("rf") != std::string::npos) found_rf = true;
    }
    REQUIRE(found_rf);
}

TEST_CASE("Device missing freq_hz returns error", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "no_freq" },
        "devices": [{ "id": "dev0", "rf": { "rate_sps": 1e6, "gain_db": 0 }}],
        "emitters": [{ "id": "em0", "device": "dev0", "waveform": {"type": "cw"} }]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    bool found = false;
    for (const auto& e : result.error()) {
        if (e.message.find("freq_hz") != std::string::npos) found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Emitter missing id returns error", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "no_emitter_id" },
        "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"device": "dev0", "waveform": {"type": "cw"}}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    bool found = false;
    for (const auto& e : result.error()) {
        if (e.message.find("emitter id") != std::string::npos) found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Invalid waveform type returns error", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "bad_waveform" },
        "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "dev0", "waveform": {"type": "nonexistent_waveform"}}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_INVALID_WAVEFORM_TYPE");
}

TEST_CASE("Parse error has Config category", "[parser][errors]") {
    auto result = parse_scenario_json("{invalid}");
    REQUIRE_FALSE(result.has_value());
    for (const auto& e : result.error()) {
        CHECK(e.category == ErrorCategory::Config);
    }
}
