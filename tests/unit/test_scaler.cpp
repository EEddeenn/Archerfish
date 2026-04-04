#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <vector>

#include "archerfish/dsp/scaler.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Scaler scales complex buffer by real factor", "[dsp][scaler]") {
    std::vector<std::complex<float>> buf = {{1.0f, 2.0f}, {3.0f, -4.0f}, {-1.0f, 0.0f}};
    scale(buf.data(), buf.size(), 2.0f);

    REQUIRE_THAT(buf[0].real(), WithinAbs(2.0f, 1e-6f));
    REQUIRE_THAT(buf[0].imag(), WithinAbs(4.0f, 1e-6f));
    REQUIRE_THAT(buf[1].real(), WithinAbs(6.0f, 1e-6f));
    REQUIRE_THAT(buf[1].imag(), WithinAbs(-8.0f, 1e-6f));
    REQUIRE_THAT(buf[2].real(), WithinAbs(-2.0f, 1e-6f));
    REQUIRE_THAT(buf[2].imag(), WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Scaler with zero factor zeros the buffer", "[dsp][scaler]") {
    std::vector<std::complex<float>> buf = {{1.0f, 2.0f}, {3.0f, -4.0f}};
    scale(buf.data(), buf.size(), 0.0f);

    for (const auto& s : buf) {
        REQUIRE_THAT(s.real(), WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(s.imag(), WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("Scaler with unit factor preserves buffer", "[dsp][scaler]") {
    std::vector<std::complex<float>> buf = {{1.5f, -2.5f}, {0.0f, 3.0f}};
    auto original = buf;
    scale(buf.data(), buf.size(), 1.0f);

    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(original[i].real(), 1e-6f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(original[i].imag(), 1e-6f));
    }
}

TEST_CASE("Scaler scales by complex factor", "[dsp][scaler]") {
    std::vector<std::complex<float>> buf = {{1.0f, 0.0f}, {0.0f, 1.0f}};
    scale(buf.data(), buf.size(), std::complex<float>{0.0f, 1.0f});

    // (1+0j) * (0+1j) = (0+1j)
    REQUIRE_THAT(buf[0].real(), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(buf[0].imag(), WithinAbs(1.0f, 1e-6f));
    // (0+1j) * (0+1j) = (-1+0j)
    REQUIRE_THAT(buf[1].real(), WithinAbs(-1.0f, 1e-6f));
    REQUIRE_THAT(buf[1].imag(), WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Scaler handles small attenuation", "[dsp][scaler]") {
    std::vector<std::complex<float>> buf = {{10.0f, 20.0f}, {-5.0f, 15.0f}};
    scale(buf.data(), buf.size(), 0.01f);

    REQUIRE_THAT(buf[0].real(), WithinAbs(0.1f, 1e-6f));
    REQUIRE_THAT(buf[0].imag(), WithinAbs(0.2f, 1e-6f));
    REQUIRE_THAT(buf[1].real(), WithinAbs(-0.05f, 1e-6f));
    REQUIRE_THAT(buf[1].imag(), WithinAbs(0.15f, 1e-6f));
}
