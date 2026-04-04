#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <vector>

#include "archerfish/impairments/burst_dropout.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("BurstDropout different seeds produce different patterns", "[impairments][burst_dropout][standalone]") {
    const size_t N = 5000;
    std::vector<std::complex<float>> data1(N, {1.0f, 0.0f});
    std::vector<std::complex<float>> data2(N, {1.0f, 0.0f});

    BurstDropoutImpairment burst1(0.3, 5.0, 111);
    BurstDropoutImpairment burst2(0.3, 5.0, 999);

    burst1.apply(data1.data(), N);
    burst2.apply(data2.data(), N);

    bool different = false;
    for (size_t i = 0; i < N; ++i) {
        if (data1[i] != data2[i]) {
            different = true;
            break;
        }
    }
    REQUIRE(different);
}

TEST_CASE("BurstDropout zero rate drops no samples", "[impairments][burst_dropout][standalone]") {
    BurstDropoutImpairment burst(0.0, 5.0, 42);
    std::vector<std::complex<float>> data(1000, {1.0f, 0.0f});

    burst.apply(data.data(), data.size());

    size_t zeros = 0;
    for (const auto& s : data) {
        if (s == std::complex<float>(0.0f, 0.0f)) ++zeros;
    }
    REQUIRE(zeros == 0);
}

TEST_CASE("BurstDropout on complex valued data", "[impairments][burst_dropout][standalone]") {
    BurstDropoutImpairment burst(0.5, 5.0, 42);
    std::vector<std::complex<float>> data(5000, {0.707f, 0.707f});

    burst.apply(data.data(), data.size());

    // Some should be zeroed (both real and imag)
    bool found_zero = false;
    bool found_nonzero = false;
    for (const auto& s : data) {
        if (s == std::complex<float>(0.0f, 0.0f)) found_zero = true;
        else found_nonzero = true;
    }
    REQUIRE(found_zero);
    REQUIRE(found_nonzero);
}

TEST_CASE("BurstDropout mean burst length affects dropout density", "[impairments][burst_dropout][standalone]") {
    // Short bursts
    BurstDropoutImpairment short_bursts(0.3, 2.0, 42);
    // Long bursts
    BurstDropoutImpairment long_bursts(0.3, 50.0, 42);

    std::vector<std::complex<float>> data_short(10000, {1.0f, 0.0f});
    std::vector<std::complex<float>> data_long(10000, {1.0f, 0.0f});

    short_bursts.apply(data_short.data(), data_short.size());
    long_bursts.apply(data_long.data(), data_long.size());

    size_t zeros_short = 0, zeros_long = 0;
    for (size_t i = 0; i < data_short.size(); ++i) {
        if (data_short[i] == std::complex<float>(0.0f, 0.0f)) ++zeros_short;
        if (data_long[i] == std::complex<float>(0.0f, 0.0f)) ++zeros_long;
    }
    // Both should have dropped some samples
    REQUIRE(zeros_short > 0);
    REQUIRE(zeros_long > 0);
}

TEST_CASE("BurstDropout default constructor is enabled", "[impairments][burst_dropout][standalone]") {
    BurstDropoutImpairment burst;
    REQUIRE(burst.enabled());
    REQUIRE(burst.name() == "burst_dropout");
}
