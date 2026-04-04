#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include <archerfish/scenario/parser.hpp>

using namespace archerfish::scenario;

// YAML tests are only compiled when yaml-cpp support is available
// These are placeholder tests that verify the parser gracefully
// reports an error when YAML is not supported

TEST_CASE("Non-YAML file parses as JSON", "[parser][json]") {
    // Write a valid JSON scenario to a .json file
    const char* json_content = R"({
        "metadata": {"name": "json_test"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 2450000000.0, "rate_sps": 10000000.0, "gain_db": 20.0}}],
        "emitters": [{"id": "e0", "device": "d0", "channel": 0, "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": {"type": "cw", "amplitude": 0.2}}]
    })";

    std::string path = "/tmp/archerfish_test_json_fallback.json";
    {
        std::ofstream f(path);
        f << json_content;
    }

    auto result = parse_scenario(path);
    REQUIRE(result.has_value());
    REQUIRE(result->metadata.name == "json_test");

    std::filesystem::remove(path);
}

TEST_CASE("YAML file produces error when yaml-cpp not available", "[yaml][parser]") {
    const char* yaml_content = R"(metadata:
  name: yaml_test
devices:
  - id: d0
    rf:
      freq_hz: 2450000000.0
      rate_sps: 10000000.0
      gain_db: 20.0
)";

    std::string path = "/tmp/archerfish_test_yaml_unsupported.yaml";
    {
        std::ofstream f(path);
        f << yaml_content;
    }

    auto result = parse_scenario(path);
    // When YAML is not compiled in, we expect an error about no YAML support
    REQUIRE_FALSE(result.has_value());

    std::filesystem::remove(path);
}

TEST_CASE("YAML file with .yml extension produces error when yaml-cpp not available", "[yaml][parser]") {
    const char* yaml_content = R"(metadata:
  name: yml_test
)";

    std::string path = "/tmp/archerfish_test_yml_unsupported.yml";
    {
        std::ofstream f(path);
        f << yaml_content;
    }

    auto result = parse_scenario(path);
    REQUIRE_FALSE(result.has_value());

    std::filesystem::remove(path);
}
