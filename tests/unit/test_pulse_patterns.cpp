#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/pulse_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Pulse with narrow width produces short bursts", "[dsp][pulse][patterns]") {
    PulseSource src;
    src.configure({{"amplitude", 1.0}, {"frequency_hz", 0.0},
                   {"pulse_width_sec", 1e-6}, {"pri_sec", 10e-6}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    int nonzero_count = 0;
    for (const auto& s : buf) {
        if (std::abs(s.real()) > 0.01f) nonzero_count++;
    }
    REQUIRE(nonzero_count > 0);
    REQUIRE(nonzero_count < 100);
}

TEST_CASE("Pulse with wide PRI has long silence periods", "[dsp][pulse][patterns]") {
    PulseSource src;
    src.configure({{"amplitude", 1.0}, {"frequency_hz", 0.0},
                   {"pulse_width_sec", 1e-6}, {"pri_sec", 100e-6}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(200);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 200);

    int zero_count = 0;
    for (const auto& s : buf) {
        if (std::abs(s.real()) < 0.01f) zero_count++;
    }
    REQUIRE(zero_count > 50);
}

TEST_CASE("Pulse duration limit respected", "[dsp][pulse][patterns]") {
    PulseSource src;
    src.configure({{"amplitude", 0.5}, {"frequency_hz", 0.0},
                   {"pulse_width_sec", 1e-6}, {"pri_sec", 10e-6},
                   {"sample_rate", 1000}, {"duration_sec", 0.01}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("Pulse reset restarts pattern", "[dsp][pulse][patterns]") {
    PulseSource src;
    src.configure({{"amplitude", 0.5}, {"frequency_hz", 0.0},
                   {"pulse_width_sec", 5e-6}, {"pri_sec", 20e-6},
                   {"sample_rate", 1e6}, {"duration_sec", 0.001}});
    src.prepare();

    std::vector<std::complex<float>> buf1(1000);
    src.render_block(buf1.data(), buf1.size());

    src.reset();

    std::vector<std::complex<float>> buf2(1000);
    size_t n = src.render_block(buf2.data(), buf2.size());
    REQUIRE(n == 1000);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf2[i].real(), WithinAbs(buf1[i].real(), 1e-6f));
    }
}

TEST_CASE("Pulse with nonzero frequency has modulated carrier", "[dsp][pulse][patterns]") {
    PulseSource src;
    src.configure({{"amplitude", 1.0}, {"frequency_hz", 100e3},
                   {"pulse_width_sec", 5e-6}, {"pri_sec", 20e-6}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    bool has_imag = false;
    for (const auto& s : buf) {
        if (std::abs(s.imag()) > 0.01f) has_imag = true;
    }
    REQUIRE(has_imag);
}
