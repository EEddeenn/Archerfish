#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#include "archerfish/dsp/chirp_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Chirp with same f0 and f1 produces CW", "[dsp][chirp][edge]") {
    ChirpSource src;
    double freq = 100e3;
    src.configure({{"amplitude", 0.5}, {"f0_hz", freq}, {"f1_hz", freq}, {"sample_rate", 1e6}, {"duration_sec", 0.001}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (size_t i = 1; i < n; ++i) {
        REQUIRE_THAT(std::abs(buf[i]), WithinAbs(std::abs(buf[0]), 0.01f));
    }

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(meta.rms_amplitude, WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(meta.crest_factor, WithinAbs(1.0, 1e-9));
}

TEST_CASE("Chirp with negative sweep", "[dsp][chirp][edge]") {
    ChirpSource src;
    src.configure({{"amplitude", 0.3}, {"f0_hz", 100e3}, {"f1_hz", -100e3}, {"sample_rate", 1e6}, {"duration_sec", 0.001}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (const auto& s : buf) {
        REQUIRE_THAT(std::abs(s), WithinAbs(0.3f, 0.01f));
    }
}

TEST_CASE("Chirp with both zero frequencies produces DC", "[dsp][chirp][edge]") {
    ChirpSource src;
    src.configure({{"amplitude", 0.2}, {"f0_hz", 0.0}, {"f1_hz", 0.0}, {"sample_rate", 1e6}, {"duration_sec", 0.001}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(0.2f, 1e-6f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("Chirp duration limit respected", "[dsp][chirp][edge]") {
    ChirpSource src;
    src.configure({{"amplitude", 0.2}, {"f0_hz", 0.0}, {"f1_hz", 100e3}, {"sample_rate", 1000}, {"duration_sec", 0.01}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("Chirp rejects non-finite sweep frequencies", "[dsp][chirp][edge]") {
    ChirpSource src;
    CHECK_THROWS_AS(src.configure({{"f0_hz", "low"}, {"f1_hz", 1.0}, {"sample_rate", 1e6}, {"duration_sec", 0.001}}),
                    std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"f0_hz", 1.0}, {"f1_hz", "high"}, {"sample_rate", 1e6}, {"duration_sec", 0.001}}),
                    std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"f0_hz", std::numeric_limits<double>::quiet_NaN()}, {"f1_hz", 1.0}, {"sample_rate", 1e6}, {"duration_sec", 0.001}}),
                    std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"f0_hz", 1.0}, {"f1_hz", std::numeric_limits<double>::infinity()}, {"sample_rate", 1e6}, {"duration_sec", 0.001}}),
                    std::invalid_argument);
}

TEST_CASE("Chirp render validates output buffer", "[dsp][chirp][edge]") {
    ChirpSource src;
    src.configure({{"sample_rate", 1e6}, {"duration_sec", 0.001}, {"f0_hz", 0.0}, {"f1_hz", 1.0}});
    src.prepare();

    CHECK(src.render_block(nullptr, 0) == 0);
    CHECK_THROWS_AS(src.render_block(nullptr, 1), std::invalid_argument);
}

TEST_CASE("Chirp reset reproduces identical output", "[dsp][chirp][edge]") {
    ChirpSource src;
    src.configure({{"amplitude", 0.5}, {"f0_hz", -50e3}, {"f1_hz", 50e3}, {"sample_rate", 1e6}, {"duration_sec", 0.001}});
    src.prepare();

    std::vector<std::complex<float>> buf1(1000);
    src.render_block(buf1.data(), buf1.size());

    src.reset();

    std::vector<std::complex<float>> buf2(1000);
    size_t n = src.render_block(buf2.data(), buf2.size());
    REQUIRE(n == 1000);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf2[i].real(), WithinAbs(buf1[i].real(), 1e-6f));
        REQUIRE_THAT(buf2[i].imag(), WithinAbs(buf1[i].imag(), 1e-6f));
    }
}
