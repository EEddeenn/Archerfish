#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/am_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("AM source zero carrier produces DC modulation envelope", "[dsp][am_source]") {
    AmSource src;
    src.configure({{"amplitude", 0.2},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 0.0},
                   {"mod_depth", 0.5},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    // With 0 Hz carrier and 0 Hz modulation, output should be constant (DC)
    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(buf[0].real(), 1e-6f));
    }
}

TEST_CASE("AM source metadata reports correct bandwidth", "[dsp][am_source]") {
    AmSource src;
    double mod_freq = 5000.0;
    src.configure({{"amplitude", 0.3},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", mod_freq},
                   {"mod_depth", 0.8},
                   {"sample_rate", 1e6}});
    src.prepare();

    auto meta = src.report_metadata();
    // AM bandwidth = 2 * mod_freq
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(2.0 * mod_freq, 1e-9));
}

TEST_CASE("AM source duration limit respected", "[dsp][am_source]") {
    AmSource src;
    src.configure({{"amplitude", 0.2},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"mod_depth", 0.5},
                   {"sample_rate", 1000},
                   {"duration_sec", 0.01}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("AM source reset restarts from beginning", "[dsp][am_source]") {
    AmSource src;
    src.configure({{"amplitude", 0.2},
                   {"carrier_freq_hz", 100e3},
                   {"mod_freq_hz", 1000.0},
                   {"mod_depth", 0.5},
                   {"sample_rate", 1e6},
                   {"duration_sec", 0.001}});
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

TEST_CASE("AM source mod depth 0 produces unmodulated carrier", "[dsp][am_source]") {
    AmSource src;
    src.configure({{"amplitude", 0.5},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"mod_depth", 0.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(1000);
    src.render_block(buf.data(), buf.size());

    // With depth=0 and carrier=0, output should be constant amplitude
    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE_THAT(std::abs(buf[i]), WithinAbs(0.5f, 0.001f));
    }
}
