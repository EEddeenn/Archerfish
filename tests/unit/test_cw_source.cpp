#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/common/constants.hpp"
#include "archerfish/dsp/cw_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("CW at 0 Hz produces constant amplitude", "[dsp][cw]") {
    CwSource src;
    src.configure({{"amplitude", 0.2}, {"frequency_hz", 0.0}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(0.2f, 1e-6f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("CW at F Hz produces correct phase progression", "[dsp][cw]") {
    CwSource src;
    double freq = 100e3;
    double fs = 1e6;
    src.configure({{"amplitude", 1.0}, {"frequency_hz", freq}, {"sample_rate", fs}});
    src.prepare();

    std::vector<std::complex<float>> buf(10);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 10);

    for (size_t i = 0; i < n; ++i) {
        double expected_phase = 2.0 * archerfish::constants::kPi * freq / fs * static_cast<double>(i);
        float expected_re = static_cast<float>(std::cos(expected_phase));
        float expected_im = static_cast<float>(std::sin(expected_phase));
        REQUIRE_THAT(buf[i].real(), WithinAbs(expected_re, 1e-5f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(expected_im, 1e-5f));
    }
}

TEST_CASE("CW metadata is correct", "[dsp][cw]") {
    CwSource src;
    src.configure({{"amplitude", 0.5}, {"sample_rate", 2e6}, {"duration_sec", 0.001}});
    src.prepare();

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(meta.sample_rate, WithinAbs(2e6, 1e-9));
    REQUIRE(meta.duration_sec.has_value());
    REQUIRE_THAT(meta.duration_sec.value(), WithinAbs(0.001, 1e-12));
    REQUIRE_FALSE(meta.repeats);
}

TEST_CASE("CW block rendering works for multiple blocks", "[dsp][cw]") {
    CwSource src;
    src.configure({{"amplitude", 0.2}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf1(50);
    std::vector<std::complex<float>> buf2(50);

    size_t n1 = src.render_block(buf1.data(), buf1.size());
    size_t n2 = src.render_block(buf2.data(), buf2.size());

    REQUIRE(n1 == 50);
    REQUIRE(n2 == 50);

    for (size_t i = 0; i < 50; ++i) {
        REQUIRE_THAT(buf2[i].real(), WithinAbs(buf1[i].real(), 1e-6f));
    }
}

TEST_CASE("CW duration limit respected", "[dsp][cw]") {
    CwSource src;
    src.configure({{"amplitude", 0.2}, {"sample_rate", 1000}, {"duration_sec", 0.01}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("CW reset restarts generation", "[dsp][cw]") {
    CwSource src;
    src.configure({{"amplitude", 0.2}, {"sample_rate", 1000}, {"duration_sec", 0.01}});
    src.prepare();

    std::vector<std::complex<float>> buf1(10);
    src.render_block(buf1.data(), buf1.size());

    size_t n = src.render_block(buf1.data(), buf1.size());
    REQUIRE(n == 0);

    src.reset();

    n = src.render_block(buf1.data(), buf1.size());
    REQUIRE(n == 10);
}
