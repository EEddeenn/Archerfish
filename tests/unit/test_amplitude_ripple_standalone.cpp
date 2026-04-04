#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/amplitude_ripple.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("AmplitudeRipple on complex non-real data", "[impairments][amplitude_ripple][standalone]") {
    AmplitudeRippleImpairment ripple(0.5, 1000.0, 10000.0);
    std::vector<std::complex<float>> data(500, {0.707f, 0.707f});
    auto original = data;

    ripple.apply(data.data(), data.size());

    // Magnitude should vary but stay close to original
    bool changed = false;
    for (size_t i = 0; i < data.size(); ++i) {
        float orig_mag = std::abs(original[i]);
        float new_mag = std::abs(data[i]);
        if (std::abs(new_mag - orig_mag) > 1e-4f) changed = true;
    }
    REQUIRE(changed);
}

TEST_CASE("AmplitudeRipple with very high frequency oscillates rapidly", "[impairments][amplitude_ripple][standalone]") {
    // High ripple frequency relative to sample rate (below Nyquist)
    AmplitudeRippleImpairment ripple(0.3, 3500.0, 8000.0);
    std::vector<std::complex<float>> data(1000, {1.0f, 0.0f});

    ripple.apply(data.data(), data.size());

    // Count sign changes in magnitude deviation to verify rapid oscillation
    int crossings = 0;
    float prev_dev = std::abs(data[0]) - 1.0f;
    for (size_t i = 1; i < data.size(); ++i) {
        float dev = std::abs(data[i]) - 1.0f;
        if ((prev_dev > 0.0f && dev < 0.0f) || (prev_dev < 0.0f && dev > 0.0f)) {
            ++crossings;
        }
        prev_dev = dev;
    }
    // Should have many zero crossings with high frequency
    REQUIRE(crossings > 10);
}

TEST_CASE("AmplitudeRipple magnitude stays within expected bounds", "[impairments][amplitude_ripple][standalone]") {
    double depth = 0.3;
    AmplitudeRippleImpairment ripple(depth, 500.0, 10000.0);
    std::vector<std::complex<float>> data(1000, {1.0f, 0.0f});

    ripple.apply(data.data(), data.size());

    for (const auto& s : data) {
        float mag = std::abs(s);
        REQUIRE(mag > 0.0f);
        REQUIRE(mag < 2.0f);
    }
}

TEST_CASE("AmplitudeRipple with different sample rates", "[impairments][amplitude_ripple][standalone]") {
    // Same frequency parameter but different sample rates
    AmplitudeRippleImpairment ripple_low(0.5, 100.0, 1000.0);
    AmplitudeRippleImpairment ripple_high(0.5, 100.0, 100000.0);

    std::vector<std::complex<float>> data_low(100, {1.0f, 0.0f});
    std::vector<std::complex<float>> data_high(100, {1.0f, 0.0f});

    ripple_low.apply(data_low.data(), data_low.size());
    ripple_high.apply(data_high.data(), data_high.size());

    // Different sample rates should produce different results
    bool different = false;
    for (size_t i = 0; i < 100; ++i) {
        if (std::abs(data_low[i].real() - data_high[i].real()) > 1e-4f) {
            different = true;
            break;
        }
    }
    REQUIRE(different);
}

TEST_CASE("AmplitudeRipple default constructor is enabled", "[impairments][amplitude_ripple][standalone]") {
    AmplitudeRippleImpairment ripple;
    REQUIRE(ripple.enabled());
    REQUIRE(ripple.name() == "amplitude_ripple");
}
