#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/phase_noise.hpp"

using namespace archerfish::impairments;
using Catch::Matchers::WithinAbs;

TEST_CASE("Phase noise name is correct") {
    PhaseNoiseImpairment pn(100.0, 0.1, 1e6);
    REQUIRE(pn.name() == "phase_noise");
}

TEST_CASE("Phase noise with zero magnitude is transparent") {
    PhaseNoiseImpairment pn(100.0, 0.0, 1e6);
    std::vector<std::complex<float>> data(100, {1.0f, 0.0f});
    auto copy = data;
    pn.apply(data.data(), data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(copy[i].real(), 1e-6f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(copy[i].imag(), 1e-6f));
    }
}

TEST_CASE("Phase noise preserves magnitude") {
    PhaseNoiseImpairment pn(100.0, 0.1, 1e6);
    const size_t N = 1000;
    std::vector<std::complex<float>> data(N, {1.0f, 0.0f});
    pn.apply(data.data(), data.size());
    for (size_t i = 0; i < N; ++i) {
        float mag = std::abs(data[i]);
        REQUIRE_THAT(mag, WithinAbs(1.0f, 1e-4f));
    }
}

TEST_CASE("Phase noise applies time-varying phase offset") {
    PhaseNoiseImpairment pn(100.0, 0.1, 1e6);
    const size_t N = 1000;
    std::vector<std::complex<float>> data(N, {1.0f, 0.0f});
    pn.apply(data.data(), data.size());
    int varying = 0;
    for (size_t i = 1; i < N; ++i) {
        float phase_prev = std::arg(data[i - 1]);
        float phase_curr = std::arg(data[i]);
        if (std::abs(phase_curr - phase_prev) > 1e-6f)
            ++varying;
    }
    REQUIRE(varying > 0);
}

TEST_CASE("Phase noise enabled/disabled") {
    PhaseNoiseImpairment pn(100.0, 0.1, 1e6);
    pn.set_enabled(false);
    std::vector<std::complex<float>> data(100, {1.0f, 0.0f});
    auto copy = data;
    pn.apply(data.data(), data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(data[i].real() == copy[i].real());
        REQUIRE(data[i].imag() == copy[i].imag());
    }
}
