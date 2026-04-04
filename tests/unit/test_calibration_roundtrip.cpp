#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/reporting/calibration.hpp"

using namespace archerfish::reporting;
using Catch::Matchers::WithinAbs;

TEST_CASE("CalibrationData to_json and from_json roundtrip", "[reporting][calibration]") {
    CalibrationData original;
    original.device_id = "usrp0";
    original.channel = 1;
    original.timestamp = "2025-01-15T10:30:00Z";
    original.entries = {
        {2.45e9, 20.0, -10.5, -10.0, -0.5},
        {3.5e9, 25.0, -8.2, -8.0, -0.2}
    };

    auto json_str = original.to_json();
    auto restored = CalibrationData::from_json(json_str);
    REQUIRE(restored.has_value());
    CHECK(restored->device_id == "usrp0");
    CHECK(restored->channel == 1);
    CHECK(restored->timestamp == "2025-01-15T10:30:00Z");
    REQUIRE(restored->entries.size() == 2);
    CHECK_THAT(restored->entries[0].freq_hz, WithinAbs(2.45e9, 1.0));
    CHECK_THAT(restored->entries[0].gain_db, WithinAbs(20.0, 1e-12));
    CHECK_THAT(restored->entries[0].measured_power_dbm, WithinAbs(-10.5, 1e-12));
    CHECK_THAT(restored->entries[1].error_db, WithinAbs(-0.2, 1e-12));
}

TEST_CASE("CalibrationData to_json is valid JSON", "[reporting][calibration]") {
    CalibrationData data;
    data.device_id = "test_dev";
    data.channel = 0;
    data.timestamp = "2025-01-01T00:00:00Z";
    data.entries = {{1e9, 10.0, -5.0, -5.0, 0.0}};

    auto json_str = data.to_json();
    REQUIRE_FALSE(json_str.empty());
    auto parsed = nlohmann::json::parse(json_str);
    CHECK(parsed.contains("device_id"));
    CHECK(parsed.contains("channel"));
    CHECK(parsed.contains("timestamp"));
    CHECK(parsed.contains("entries"));
}

TEST_CASE("CalibrationData from_json invalid returns error", "[reporting][calibration]") {
    auto result = CalibrationData::from_json("not valid json {{{");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("CalibrationData empty entries roundtrip", "[reporting][calibration]") {
    CalibrationData data;
    data.device_id = "empty";
    data.channel = 0;
    data.timestamp = "2025-06-01";

    auto json_str = data.to_json();
    auto restored = CalibrationData::from_json(json_str);
    REQUIRE(restored.has_value());
    CHECK(restored->device_id == "empty");
    CHECK(restored->entries.empty());
}

TEST_CASE("CalibrationData interpolation with data points", "[reporting][calibration]") {
    CalibrationData data;
    data.device_id = "interp_test";
    data.channel = 0;
    data.timestamp = "";
    data.entries = {
        {1e9, 10.0, -20.0, -20.0, 0.0},
        {2e9, 10.0, -18.0, -18.0, 0.0}
    };

    auto result = data.interpolate_power(1.5e9, 10.0);
    CHECK(result.has_value());
}
