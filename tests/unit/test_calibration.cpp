#include <catch2/catch_test_macros.hpp>
#include <archerfish/reporting/calibration.hpp>

using namespace archerfish::reporting;

TEST_CASE("CalibrationData round-trips through JSON", "[reporting][calibration]") {
    CalibrationData original;
    original.device_id = "test_device";
    original.channel = 0;
    original.timestamp = "2026-04-04T00:00:00Z";
    original.entries.push_back({2.4e9, 20.0, -10.5, -10.0, -0.5});
    original.entries.push_back({3.5e9, 25.0, -8.0, -8.0, 0.0});

    std::string json = original.to_json();
    auto result = CalibrationData::from_json(json);
    REQUIRE(result.has_value());

    const auto& parsed = result.value();
    REQUIRE(parsed.device_id == "test_device");
    REQUIRE(parsed.channel == 0);
    REQUIRE(parsed.timestamp == "2026-04-04T00:00:00Z");
    REQUIRE(parsed.entries.size() == 2);
    REQUIRE(parsed.entries[0].freq_hz == 2.4e9);
    REQUIRE(parsed.entries[0].gain_db == 20.0);
    REQUIRE(parsed.entries[0].measured_power_dbm == -10.5);
    REQUIRE(parsed.entries[0].expected_power_dbm == -10.0);
    REQUIRE(parsed.entries[0].error_db == -0.5);
    REQUIRE(parsed.entries[1].freq_hz == 3.5e9);
}

TEST_CASE("CalibrationData rejects malformed JSON", "[reporting][calibration]") {
    auto result = CalibrationData::from_json("not json at all");
    REQUIRE_FALSE(result.has_value());

    auto result2 = CalibrationData::from_json("{\"device_id\": 123}");
    REQUIRE_FALSE(result2.has_value());

    auto result3 = CalibrationData::from_json("{}");
    REQUIRE_FALSE(result3.has_value());
}

TEST_CASE("CalibrationData handles empty entries", "[reporting][calibration]") {
    CalibrationData data;
    data.device_id = "dev0";
    data.channel = 1;
    data.timestamp = "2026-04-04T12:00:00Z";

    std::string json = data.to_json();
    auto result = CalibrationData::from_json(json);
    REQUIRE(result.has_value());
    REQUIRE(result->entries.empty());
}

TEST_CASE("calibration_dir returns a path under .config/archerfish", "[reporting][calibration]") {
    auto dir = CalibrationData::calibration_dir();
    REQUIRE(dir.string().find(".config/archerfish/calibration") != std::string::npos);
}

TEST_CASE("calibration_file formats correctly", "[reporting][calibration]") {
    auto path = CalibrationData::calibration_file("usrp0", 2);
    REQUIRE(path.filename() == "usrp0_ch2.json");
}

TEST_CASE("CalibrationPoint with missing fields is rejected", "[reporting][calibration]") {
    std::string bad = R"({
        "device_id": "d",
        "channel": 0,
        "timestamp": "2026-01-01T00:00:00Z",
        "entries": [{"freq_hz": 1.0}]
    })";
    auto result = CalibrationData::from_json(bad);
    REQUIRE_FALSE(result.has_value());
}
