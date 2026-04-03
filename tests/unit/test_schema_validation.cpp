#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "archerfish/scenario/schema_validator.hpp"

#include <filesystem>
#include <fstream>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::ContainsSubstring;

static const char* examples_dir = EXAMPLES_DIR;

static nlohmann::json load_json_file(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    REQUIRE(ifs.is_open());
    return nlohmann::json::parse(ifs);
}

static nlohmann::json make_valid_scenario() {
    return R"({
        "metadata": { "name": "test" },
        "devices": [{
            "id": "usrp0",
            "channel": 0,
            "rf": {
                "freq_hz": 2450000000.0,
                "rate_sps": 10000000.0,
                "gain_db": 20.0
            }
        }],
        "emitters": [{
            "id": "cw1",
            "device": "usrp0",
            "channel": 0,
            "start_after_sec": 0.5,
            "duration_sec": 1.0,
            "waveform": { "type": "cw", "amplitude": 0.2 }
        }]
    })"_json;
}

TEST_CASE("Schema loads and is valid JSON", "[schema_validation]") {
    auto schema = get_scenario_schema();
    REQUIRE(schema.is_object());
    REQUIRE(schema.contains("$schema"));
    REQUIRE(schema["$schema"].get<std::string>() == "http://json-schema.org/draft-07/schema#");
    REQUIRE(schema.contains("properties"));
    REQUIRE(schema["properties"].contains("metadata"));
    REQUIRE(schema["properties"].contains("devices"));
    REQUIRE(schema["properties"].contains("emitters"));
}

TEST_CASE("Valid scenario passes validation", "[schema_validation]") {
    auto instance = make_valid_scenario();
    auto result = validate_schema(instance);
    REQUIRE(result.has_value());
}

TEST_CASE("Missing required 'devices' produces error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "emitters": []
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
}

TEST_CASE("Missing required 'metadata' produces error", "[schema_validation]") {
    auto instance = R"({
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [{"id": "e1", "device": "usrp0"}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
}

TEST_CASE("Missing required 'emitters' produces error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
}

TEST_CASE("Invalid waveform type produces error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [{
            "id": "e1", "device": "usrp0",
            "waveform": { "type": "invalid_type", "amplitude": 0.2 }
        }]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
    bool found_type_error = false;
    for (const auto& e : result.error()) {
        if (e.message.find("type") != std::string::npos) found_type_error = true;
    }
    REQUIRE(found_type_error);
}

TEST_CASE("Negative freq_hz produces error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": -1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [{"id": "e1", "device": "usrp0", "waveform": {"type": "cw"}}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
    bool found_freq_error = false;
    for (const auto& e : result.error()) {
        if (e.message.find("freq_hz") != std::string::npos) found_freq_error = true;
    }
    REQUIRE(found_freq_error);
}

TEST_CASE("Zero freq_hz produces error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 0, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [{"id": "e1", "device": "usrp0", "waveform": {"type": "cw"}}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Negative rate_sps produces error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": -5e6, "gain_db": 10}}],
        "emitters": [{"id": "e1", "device": "usrp0", "waveform": {"type": "cw"}}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Emitter missing both waveform and waveform_ref passes schema", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [{"id": "e1", "device": "usrp0"}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE(result.has_value());
}

TEST_CASE("Emitter with waveform_ref passes", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "waveforms": [{"id": "my_cw", "type": "cw", "amplitude": 0.5}],
        "emitters": [{"id": "e1", "device": "usrp0", "waveform_ref": "my_cw"}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE(result.has_value());
}

TEST_CASE("Valid scenario with impairments passes", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "impairment_test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20}}],
        "emitters": [{
            "id": "e1",
            "device": "usrp0",
            "channel": 0,
            "start_after_sec": 0.0,
            "duration_sec": 1.0,
            "waveform": { "type": "cw", "amplitude": 0.2 },
            "impairments": {
                "cfo_hz": 200.0,
                "phase_offset_rad": 0.1,
                "iq_gain_imbalance_db": 0.5,
                "iq_phase_imbalance_rad": 0.02,
                "dc_offset_i": 0.01,
                "dc_offset_q": -0.01,
                "awgn_power": 0.001
            }
        }]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE(result.has_value());
}

TEST_CASE("Scenario with reporting passes", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "reporting_test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [{"id": "e1", "device": "usrp0", "waveform": {"type": "noise"}}],
        "reporting": { "save_plan": true, "save_metrics": true }
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE(result.has_value());
}

TEST_CASE("Empty devices array produces error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [],
        "emitters": [{"id": "e1", "device": "usrp0", "waveform": {"type": "cw"}}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Empty emitters array produces error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": []
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("All schema errors use ErrorCategory::Config", "[schema_validation]") {
    auto instance = R"({
        "metadata": {},
        "devices": [],
        "emitters": []
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
    for (const auto& e : result.error()) {
        CHECK(e.category == ErrorCategory::Config);
    }
}

TEST_CASE("All waveform types are accepted by schema", "[schema_validation]") {
    const std::vector<std::string> types = {
        "cw", "chirp", "noise", "qpsk", "bpsk",
        "8psk", "qam16", "qam64", "multi_tone", "file"
    };
    for (const auto& t : types) {
        auto instance = nlohmann::json::parse(fmt::format(R"({{
            "metadata": {{ "name": "type_test" }},
            "devices": [{{"id": "usrp0", "rf": {{"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}}}],
            "emitters": [{{"id": "e1", "device": "usrp0", "waveform": {{"type": "{}"}}}}]
        }})", t));
        auto result = validate_schema(instance);
        INFO("Waveform type: " << t);
        CHECK(result.has_value());
    }
}

TEST_CASE("Device missing rf produces error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0"}],
        "emitters": [{"id": "e1", "device": "usrp0", "waveform": {"type": "cw"}}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Device with bandwidth_hz and antenna passes", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{
            "id": "usrp0",
            "channel": 0,
            "rf": {
                "freq_hz": 2450000000.0,
                "rate_sps": 10000000.0,
                "gain_db": 20.0,
                "bandwidth_hz": 8000000.0,
                "antenna": "TX/RX"
            }
        }],
        "emitters": [{"id": "e1", "device": "usrp0", "waveform": {"type": "cw"}}]
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE(result.has_value());
}

TEST_CASE("All example files validate against schema", "[schema_validation]") {
    const std::vector<std::string> examples = {
        "future_start_cw.json",
        "chirp_burst.json",
        "qpsk_burst.json",
        "mixed_scene.json",
    };
    for (const auto& filename : examples) {
        auto path = std::filesystem::path(examples_dir) / filename;
        INFO("Example file: " << filename);
        auto instance = load_json_file(path);
        auto result = validate_schema(instance);
        CHECK(result.has_value());
    }
}

TEST_CASE("Unknown additional properties at root produce error", "[schema_validation]") {
    auto instance = R"({
        "metadata": { "name": "test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [{"id": "e1", "device": "usrp0", "waveform": {"type": "cw"}}],
        "unknown_field": true
    })"_json;
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Non-object root produces error", "[schema_validation]") {
    auto instance = nlohmann::json::array();
    auto result = validate_schema(instance);
    REQUIRE_FALSE(result.has_value());
}
