#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/fading.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("Fading high Doppler has more variation than low Doppler", "[fading][models]") {
    const size_t N = 10000;
    std::vector<std::complex<float>> data_low(N, {1.0f, 0.0f});
    std::vector<std::complex<float>> data_high(N, {1.0f, 0.0f});

    FadingImpairment fade_low(1.0, 1e6, "rayleigh");
    FadingImpairment fade_high(1000.0, 1e6, "rayleigh");

    fade_low.apply(data_low.data(), N);
    fade_high.apply(data_high.data(), N);

    auto variance = [](const std::vector<std::complex<float>>& d) {
        double mean = 0.0;
        for (const auto& s : d) mean += std::abs(s);
        mean /= static_cast<double>(d.size());
        double var = 0.0;
        for (const auto& s : d) {
            double diff = std::abs(s) - mean;
            var += diff * diff;
        }
        return var / static_cast<double>(d.size());
    };

    REQUIRE(variance(data_high) > variance(data_low));
}

TEST_CASE("Fading Rician high K-factor has less variance than low K-factor", "[fading][models]") {
    const size_t N = 10000;
    std::vector<std::complex<float>> data_k0(N, {1.0f, 0.0f});
    std::vector<std::complex<float>> data_k100(N, {1.0f, 0.0f});

    FadingImpairment fade_k0(100.0, 1e6, "rayleigh");
    FadingImpairment fade_k100(100.0, 1e6, "rician", 100.0);

    fade_k0.apply(data_k0.data(), N);
    fade_k100.apply(data_k100.data(), N);

    auto variance = [](const std::vector<std::complex<float>>& d) {
        double mean = 0.0;
        for (const auto& s : d) mean += std::abs(s);
        mean /= static_cast<double>(d.size());
        double var = 0.0;
        for (const auto& s : d) {
            double diff = std::abs(s) - mean;
            var += diff * diff;
        }
        return var / static_cast<double>(d.size());
    };

    REQUIRE(variance(data_k100) < variance(data_k0));
}

TEST_CASE("Fading preserves zero input", "[fading][models]") {
    FadingImpairment fade(100.0, 1e6, "rayleigh");
    std::vector<std::complex<float>> data(100, {0.0f, 0.0f});

    fade.apply(data.data(), data.size());

    for (const auto& s : data) {
        REQUIRE_THAT(s.real(), WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(s.imag(), WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("Fading name returns correct value", "[fading][models]") {
    FadingImpairment fade(100.0, 1e6, "rayleigh");
    REQUIRE(fade.name() == "fading");

    FadingImpairment fade2(100.0, 1e6, "rician", 5.0);
    REQUIRE(fade2.name() == "fading");
}

TEST_CASE("Fading Rician very high K-factor approaches constant magnitude", "[fading][models]") {
    const size_t N = 10000;
    std::vector<std::complex<float>> data(N, {1.0f, 0.0f});

    FadingImpairment fade(100.0, 1e6, "rician", 100.0);
    fade.apply(data.data(), N);

    double mean_mag = 0.0;
    for (size_t i = 0; i < N; ++i) mean_mag += std::abs(data[i]);
    mean_mag /= static_cast<double>(N);

    double max_deviation = 0.0;
    for (size_t i = 0; i < N; ++i) {
        double dev = std::abs(std::abs(data[i]) - mean_mag);
        max_deviation = std::max(max_deviation, dev);
    }
    REQUIRE(max_deviation < 2.0);
}
