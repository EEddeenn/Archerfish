#include <catch2/catch_test_macros.hpp>
#include <archerfish/reporting/calibration.hpp>

#include <limits>

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

TEST_CASE("calibration_file sanitizes unsafe device ID path characters", "[reporting][calibration]") {
    auto path = CalibrationData::calibration_file("../escape\\dev\n", 2);
    REQUIRE(path.parent_path() == CalibrationData::calibration_dir());
    REQUIRE(path.filename() == ".._escape_dev__ch2.json");
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

TEST_CASE("CalibrationData rejects invalid entry types and values", "[reporting][calibration]") {
    std::string bad_type = R"({
        "device_id": "d",
        "channel": 0,
        "timestamp": "2026-01-01T00:00:00Z",
        "entries": [{
            "freq_hz": "1e9",
            "gain_db": 10.0,
            "measured_power_dbm": -10.0,
            "expected_power_dbm": -9.0,
            "error_db": -1.0
        }]
    })";
    auto type_result = CalibrationData::from_json(bad_type);
    REQUIRE_FALSE(type_result.has_value());

    CalibrationData with_nan;
    with_nan.device_id = "d";
    with_nan.channel = 0;
    with_nan.timestamp = "2026-01-01T00:00:00Z";
    with_nan.entries.push_back({std::numeric_limits<double>::quiet_NaN(), 10.0, -10.0, -9.0, -1.0});
    auto nan_result = CalibrationData::from_json(with_nan.to_json());
    REQUIRE_FALSE(nan_result.has_value());

    std::string negative_freq = R"({
        "device_id": "d",
        "channel": 0,
        "timestamp": "2026-01-01T00:00:00Z",
        "entries": [{
            "freq_hz": -1.0,
            "gain_db": 10.0,
            "measured_power_dbm": -10.0,
            "expected_power_dbm": -9.0,
            "error_db": -1.0
        }]
    })";
    auto freq_result = CalibrationData::from_json(negative_freq);
    REQUIRE_FALSE(freq_result.has_value());
}

TEST_CASE("CalibrationData rejects empty identity fields", "[reporting][calibration]") {
    std::string empty_device = R"({
        "device_id": "",
        "channel": 0,
        "timestamp": "2026-01-01T00:00:00Z",
        "entries": []
    })";
    REQUIRE_FALSE(CalibrationData::from_json(empty_device).has_value());

    std::string empty_timestamp = R"({
        "device_id": "d",
        "channel": 0,
        "timestamp": "",
        "entries": []
    })";
    REQUIRE_FALSE(CalibrationData::from_json(empty_timestamp).has_value());
}

TEST_CASE("CalibrationData rejects blank identity fields", "[reporting][calibration]") {
    std::string blank_device = R"({
        "device_id": " \t\n",
        "channel": 0,
        "timestamp": "2026-01-01T00:00:00Z",
        "entries": []
    })";
    REQUIRE_FALSE(CalibrationData::from_json(blank_device).has_value());

    std::string blank_timestamp = R"({
        "device_id": "d",
        "channel": 0,
        "timestamp": " \t\n",
        "entries": []
    })";
    REQUIRE_FALSE(CalibrationData::from_json(blank_timestamp).has_value());
}

TEST_CASE("CalibrationData rejects channel values outside uint32 range", "[reporting][calibration]") {
    std::string out_of_range = R"({
        "device_id": "d",
        "channel": 4294967296,
        "timestamp": "2026-01-01T00:00:00Z",
        "entries": []
    })";

    auto result = CalibrationData::from_json(out_of_range);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Calibration interpolation rejects non-finite queries and entries", "[reporting][calibration]") {
    CalibrationData data;
    data.device_id = "dev0";
    data.channel = 0;
    data.timestamp = "2026-01-01T00:00:00Z";
    data.entries.push_back({1e9, 10.0, -20.0, -20.0, 0.0});

    CHECK_FALSE(data.interpolate_power(std::numeric_limits<double>::quiet_NaN(), 10.0).has_value());
    CHECK_FALSE(data.interpolate_power(1e9, std::numeric_limits<double>::infinity()).has_value());
    CHECK_FALSE(data.interpolate_power(-1.0, 10.0).has_value());
    CHECK(data.interpolate_power(1e9, 10.0).has_value());

    CalibrationData bad;
    bad.device_id = "dev0";
    bad.channel = 0;
    bad.timestamp = "2026-01-01T00:00:00Z";
    bad.entries.push_back({1e9, 10.0, std::numeric_limits<double>::quiet_NaN(), -20.0, 0.0});
    CHECK_FALSE(bad.interpolate_power(1e9, 10.0).has_value());
}

TEST_CASE("Calibration required amplitude rejects non-finite inputs and outputs", "[reporting][calibration]") {
    CalibrationData data;
    data.device_id = "dev0";
    data.channel = 0;
    data.timestamp = "2026-01-01T00:00:00Z";
    data.entries.push_back({1e9, 10.0, -20.0, -20.0, 0.0});

    CHECK(data.compute_required_amplitude(std::numeric_limits<double>::quiet_NaN(), 1e9, 10.0) == -1.0);
    CHECK(data.compute_required_amplitude(0.0, std::numeric_limits<double>::infinity(), 10.0) == -1.0);
    CHECK(data.compute_required_amplitude(std::numeric_limits<double>::max(), 1e9, 10.0) == -1.0);
}
