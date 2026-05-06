#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>
#include <vector>

#include "archerfish/dsp/scaler.hpp"
#include "archerfish/dsp/summer.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Scaler multiply by 2.0 doubles amplitude", "[dsp][scaler]") {
    std::vector<std::complex<float>> buf = {{1.0f, 0.0f}, {0.0f, 1.0f}, {0.5f, 0.5f}};
    scale(buf.data(), buf.size(), 2.0f);

    REQUIRE_THAT(buf[0].real(), WithinAbs(2.0f, 1e-6f));
    REQUIRE_THAT(buf[0].imag(), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(buf[1].real(), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(buf[1].imag(), WithinAbs(2.0f, 1e-6f));
    REQUIRE_THAT(buf[2].real(), WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(buf[2].imag(), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Scaler complex rotation by exp(j*pi/4)", "[dsp][scaler]") {
    float sq2 = std::sqrt(2.0f) / 2.0f;
    std::complex<float> rot{sq2, sq2};

    std::vector<std::complex<float>> buf = {{1.0f, 0.0f}};
    scale(buf.data(), buf.size(), rot);

    REQUIRE_THAT(buf[0].real(), WithinAbs(sq2, 1e-6f));
    REQUIRE_THAT(buf[0].imag(), WithinAbs(sq2, 1e-6f));
}

TEST_CASE("Scaler rejects invalid buffers and factors", "[dsp][scaler]") {
    std::vector<std::complex<float>> buf = {{1.0f, 0.0f}};

    REQUIRE_NOTHROW(scale(nullptr, 0, 1.0f));
    REQUIRE_THROWS_AS(scale(nullptr, 1, 1.0f), std::invalid_argument);
    REQUIRE_THROWS_AS(scale(buf.data(), buf.size(), std::numeric_limits<float>::infinity()),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(scale(buf.data(), buf.size(),
                            std::complex<float>{1.0f, std::numeric_limits<float>::quiet_NaN()}),
                      std::invalid_argument);
}

TEST_CASE("Summer adds two constant buffers", "[dsp][summer]") {
    std::vector<std::complex<float>> a(5, {1.0f, 0.5f});
    std::vector<std::complex<float>> b(5, {0.5f, 1.0f});
    std::vector<std::complex<float>> out(5);

    summer(a.data(), b.data(), out.data(), out.size());

    for (size_t i = 0; i < out.size(); ++i) {
        REQUIRE_THAT(out[i].real(), WithinAbs(1.5f, 1e-6f));
        REQUIRE_THAT(out[i].imag(), WithinAbs(1.5f, 1e-6f));
    }
}

TEST_CASE("Summer headroom warning when sum exceeds 1.0", "[dsp][summer]") {
    std::vector<std::complex<float>> a(3, {0.8f, 0.0f});
    std::vector<std::complex<float>> b(3, {0.8f, 0.0f});
    std::vector<std::complex<float>> out(3);

    bool exceeds = summer_with_headroom_check(a.data(), b.data(), out.data(), out.size());

    REQUIRE(exceeds);
    for (size_t i = 0; i < out.size(); ++i) {
        REQUIRE_THAT(out[i].real(), WithinAbs(1.6f, 1e-6f));
    }
}

TEST_CASE("Summer rejects null buffers for nonzero work", "[dsp][summer]") {
    std::vector<std::complex<float>> a(1, {1.0f, 0.0f});
    std::vector<std::complex<float>> b(1, {1.0f, 0.0f});
    std::vector<std::complex<float>> out(1);

    REQUIRE_NOTHROW(summer(nullptr, nullptr, nullptr, 0));
    REQUIRE_NOTHROW(summer_with_headroom_check(nullptr, nullptr, nullptr, 0));
    REQUIRE_THROWS_AS(summer(nullptr, b.data(), out.data(), 1), std::invalid_argument);
    REQUIRE_THROWS_AS(summer(a.data(), nullptr, out.data(), 1), std::invalid_argument);
    REQUIRE_THROWS_AS(summer(a.data(), b.data(), nullptr, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(summer_with_headroom_check(nullptr, b.data(), out.data(), 1),
                      std::invalid_argument);
}
