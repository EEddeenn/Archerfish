#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/cw_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("CW at near-unity amplitude", "[dsp][cw][edge]") {
    CwSource src;
    src.configure({{"amplitude", 0.999}, {"frequency_hz", 0.0}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(std::abs(buf[i]), WithinAbs(0.999f, 1e-5f));
    }
}

TEST_CASE("CW at very small amplitude", "[dsp][cw][edge]") {
    CwSource src;
    src.configure({{"amplitude", 1e-6}, {"frequency_hz", 0.0}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(1e-6f, 1e-9f));
    }
}

TEST_CASE("CW at very high frequency near Nyquist", "[dsp][cw][edge]") {
    CwSource src;
    double fs = 1e6;
    double freq = 0.499 * fs;
    src.configure({{"amplitude", 1.0}, {"frequency_hz", freq}, {"sample_rate", fs}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (const auto& s : buf) {
        REQUIRE(std::abs(s) <= 1.1f);
    }
}

TEST_CASE("CW at very low frequency", "[dsp][cw][edge]") {
    CwSource src;
    double fs = 1e6;
    double freq = 1.0;
    src.configure({{"amplitude", 1.0}, {"frequency_hz", freq}, {"sample_rate", fs}});
    src.prepare();

    std::vector<std::complex<float>> buf(1000);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 1000);

    double phase_diff = 2.0 * M_PI * freq / fs;
    REQUIRE_THAT(buf[1].real(), WithinAbs(std::cos(phase_diff), 1e-5));
}

TEST_CASE("CW with zero amplitude produces silence", "[dsp][cw][edge]") {
    CwSource src;
    src.configure({{"amplitude", 0.0}, {"frequency_hz", 100e3}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (const auto& s : buf) {
        REQUIRE_THAT(s.real(), WithinAbs(0.0f, 1e-9f));
        REQUIRE_THAT(s.imag(), WithinAbs(0.0f, 1e-9f));
    }
}
