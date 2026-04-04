#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/fading.hpp"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("FadingImpairment name", "[fading]") {
    archerfish::impairments::FadingImpairment fade(100.0, 1e6);
    REQUIRE(fade.name() == "fading");
}

TEST_CASE("FadingImpairment applies time-varying gain", "[fading]") {
    double sample_rate = 1e6;
    double doppler = 100.0;
    archerfish::impairments::FadingImpairment fade(doppler, sample_rate);

    size_t N = 10000;
    std::vector<std::complex<float>> data(N, {1.0f, 0.0f});
    fade.apply(data.data(), N);

    bool has_variation = false;
    float first_mag = std::abs(data[0]);
    for (size_t i = 1; i < N; ++i) {
        if (std::abs(std::abs(data[i]) - first_mag) > 0.01f) {
            has_variation = true;
            break;
        }
    }
    REQUIRE(has_variation);
}

TEST_CASE("FadingImpairment with zero doppler is approximately constant", "[fading]") {
    double sample_rate = 1e6;
    double doppler = 0.0;
    archerfish::impairments::FadingImpairment fade(doppler, sample_rate);

    size_t N = 1000;
    std::vector<std::complex<float>> data(N, {1.0f, 0.0f});
    fade.apply(data.data(), N);

    float first_mag = std::abs(data[0]);
    for (size_t i = 1; i < N; ++i) {
        REQUIRE_THAT(std::abs(data[i]), WithinAbs(first_mag, 0.01f));
    }
}

TEST_CASE("FadingImpairment Rayleigh has zero-centered mean", "[fading]") {
    double sample_rate = 1e6;
    double doppler = 500.0;
    archerfish::impairments::FadingImpairment fade(doppler, sample_rate, "rayleigh", 0.0);

    size_t N = 100000;
    std::vector<std::complex<float>> data(N, {1.0f, 0.0f});
    fade.apply(data.data(), N);

    double sum_re = 0.0, sum_im = 0.0;
    for (size_t i = 0; i < N; ++i) {
        sum_re += data[i].real();
        sum_im += data[i].imag();
    }
    double mean_re = sum_re / static_cast<double>(N);
    double mean_im = sum_im / static_cast<double>(N);

    REQUIRE_THAT(mean_re, WithinAbs(0.0, 0.1));
    REQUIRE_THAT(mean_im, WithinAbs(0.0, 0.1));
}

TEST_CASE("FadingImpairment Rician with high K-factor approaches constant", "[fading]") {
    double sample_rate = 1e6;
    double doppler = 100.0;
    double k_factor = 100.0;
    archerfish::impairments::FadingImpairment fade(doppler, sample_rate, "rician", k_factor);

    size_t N = 10000;
    std::vector<std::complex<float>> data(N, {1.0f, 0.0f});
    fade.apply(data.data(), N);

    double mean_mag = 0.0;
    for (size_t i = 0; i < N; ++i) {
        mean_mag += std::abs(data[i]);
    }
    mean_mag /= static_cast<double>(N);

    double variance = 0.0;
    for (size_t i = 0; i < N; ++i) {
        double diff = std::abs(data[i]) - mean_mag;
        variance += diff * diff;
    }
    variance /= static_cast<double>(N);

    REQUIRE(variance < 0.1);
}

TEST_CASE("FadingImpairment disabled produces no effect", "[fading]") {
    archerfish::impairments::FadingImpairment fade(100.0, 1e6);
    fade.set_enabled(false);

    std::vector<std::complex<float>> data(100, {1.0f, 0.0f});
    fade.apply(data.data(), 100);

    for (size_t i = 0; i < 100; ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("FadingImpairment Rician with K=0 equals Rayleigh", "[fading]") {
    double sample_rate = 1e6;
    double doppler = 100.0;
    archerfish::impairments::FadingImpairment rayleigh(doppler, sample_rate, "rayleigh", 0.0);
    archerfish::impairments::FadingImpairment rician_k0(doppler, sample_rate, "rician", 0.0);

    size_t N = 1000;
    std::vector<std::complex<float>> data_r(N, {1.0f, 0.0f});
    std::vector<std::complex<float>> data_k(N, {1.0f, 0.0f});

    rayleigh.apply(data_r.data(), N);
    rician_k0.apply(data_k.data(), N);

    for (size_t i = 0; i < N; ++i) {
        REQUIRE_THAT(data_r[i].real(), WithinAbs(data_k[i].real(), 1e-5f));
        REQUIRE_THAT(data_r[i].imag(), WithinAbs(data_k[i].imag(), 1e-5f));
    }
}
