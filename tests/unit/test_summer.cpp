#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <vector>

#include "archerfish/dsp/summer.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Summer adds two identical buffers element-wise", "[dsp][summer]") {
    std::vector<std::complex<float>> a(8, {1.0f, 0.0f});
    std::vector<std::complex<float>> b(8, {0.5f, 0.5f});
    std::vector<std::complex<float>> out(8);

    summer(a.data(), b.data(), out.data(), 8);

    for (size_t i = 0; i < 8; ++i) {
        REQUIRE_THAT(out[i].real(), WithinAbs(1.5f, 1e-6f));
        REQUIRE_THAT(out[i].imag(), WithinAbs(0.5f, 1e-6f));
    }
}

TEST_CASE("Summer produces zero output when inputs cancel", "[dsp][summer]") {
    std::vector<std::complex<float>> a(4, {1.0f, -2.0f});
    std::vector<std::complex<float>> b(4, {-1.0f, 2.0f});
    std::vector<std::complex<float>> out(4);

    summer(a.data(), b.data(), out.data(), 4);

    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_THAT(out[i].real(), WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(out[i].imag(), WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("Summer handles single element", "[dsp][summer]") {
    std::complex<float> a{3.0f, -1.0f};
    std::complex<float> b{-2.0f, 4.0f};
    std::complex<float> out;

    summer(&a, &b, &out, 1);

    REQUIRE_THAT(out.real(), WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(out.imag(), WithinAbs(3.0f, 1e-6f));
}

TEST_CASE("Summer with headroom check returns true when no clipping", "[dsp][summer]") {
    std::vector<std::complex<float>> a(4, {0.3f, 0.3f});
    std::vector<std::complex<float>> b(4, {0.3f, 0.3f});
    std::vector<std::complex<float>> out(4);

    bool exceeds = summer_with_headroom_check(a.data(), b.data(), out.data(), 4);

    REQUIRE_FALSE(exceeds);
    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_THAT(out[i].real(), WithinAbs(0.6f, 1e-6f));
        REQUIRE_THAT(out[i].imag(), WithinAbs(0.6f, 1e-6f));
    }
}

TEST_CASE("Summer with headroom check returns false when clipping", "[dsp][summer]") {
    std::vector<std::complex<float>> a(4, {0.9f, 0.9f});
    std::vector<std::complex<float>> b(4, {0.9f, 0.9f});
    std::vector<std::complex<float>> out(4);

    bool exceeds = summer_with_headroom_check(a.data(), b.data(), out.data(), 4);

    REQUIRE(exceeds);

    for (size_t i = 0; i < 4; ++i) {
        REQUIRE_THAT(out[i].real(), WithinAbs(1.8f, 1e-6f));
        REQUIRE_THAT(out[i].imag(), WithinAbs(1.8f, 1e-6f));
    }
}
