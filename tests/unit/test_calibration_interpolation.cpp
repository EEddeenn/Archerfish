#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <archerfish/reporting/calibration.hpp>

using namespace archerfish::reporting;

TEST_CASE("Interpolation returns nullopt for empty calibration", "[calibration][interpolation]") {
    CalibrationData data;
    data.device_id = "dev0";
    data.channel = 0;
    data.timestamp = "2026-04-04T00:00:00Z";

    auto result = data.interpolate_power(2.4e9, 20.0);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Nearest-neighbor interpolation picks closest point", "[calibration][interpolation]") {
    CalibrationData data;
    data.device_id = "dev0";
    data.channel = 0;
    data.timestamp = "2026-04-04T00:00:00Z";
    data.entries.push_back({2.4e9, 20.0, -10.0, -10.0, 0.0});
    data.entries.push_back({3.5e9, 25.0, -8.0, -8.0, 0.0});

    auto result = data.interpolate_power(2.5e9, 20.0);
    REQUIRE(result.has_value());
    REQUIRE(result->method == "nearest_neighbor");
    REQUIRE(result->estimated_power_dbm == -10.0);
}

TEST_CASE("Bilinear interpolation with 4 bracketing entries", "[calibration][interpolation]") {
    CalibrationData data;
    data.device_id = "dev0";
    data.channel = 0;
    data.timestamp = "2026-04-04T00:00:00Z";
    data.entries.push_back({2.0e9, 10.0, -20.0, -20.0, 0.0});
    data.entries.push_back({3.0e9, 10.0, -18.0, -18.0, 0.0});
    data.entries.push_back({2.0e9, 20.0, -10.0, -10.0, 0.0});
    data.entries.push_back({3.0e9, 20.0, -8.0, -8.0, 0.0});

    auto result = data.interpolate_power(2.5e9, 15.0);
    REQUIRE(result.has_value());
    REQUIRE(result->method == "bilinear");
    REQUIRE_THAT(result->estimated_power_dbm, Catch::Matchers::WithinAbs(-14.0, 0.5));
}

TEST_CASE("Compute required amplitude returns -1 for empty calibration", "[calibration][interpolation]") {
    CalibrationData data;
    data.device_id = "dev0";
    data.channel = 0;
    data.timestamp = "2026-04-04T00:00:00Z";

    double amp = data.compute_required_amplitude(-10.0, 2.4e9, 20.0);
    REQUIRE(amp == -1.0);
}
