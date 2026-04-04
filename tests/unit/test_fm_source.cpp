#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/fm_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("FM source produces constant amplitude", "[dsp][fm_source]") {
    FmSource src;
    src.configure({{"amplitude", 0.3},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(5000);
    src.render_block(buf.data(), buf.size());

    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE_THAT(std::abs(buf[i]), WithinAbs(0.3f, 0.005f));
    }
}

TEST_CASE("FM source metadata reports Carson's bandwidth", "[dsp][fm_source]") {
    FmSource src;
    double mod_freq = 2000.0;
    double deviation = 10000.0;
    src.configure({{"amplitude", 0.2},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", mod_freq},
                   {"deviation_hz", deviation},
                   {"sample_rate", 1e6}});
    src.prepare();

    auto meta = src.report_metadata();
    double expected_bw = 2.0 * (deviation + mod_freq);
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(expected_bw, 1e-9));
}

TEST_CASE("FM source duration limit respected", "[dsp][fm_source]") {
    FmSource src;
    src.configure({{"amplitude", 0.2},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1000},
                   {"duration_sec", 0.01}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("FM source reset produces identical output", "[dsp][fm_source]") {
    FmSource src;
    src.configure({{"amplitude", 0.5},
                   {"carrier_freq_hz", 50e3},
                   {"mod_freq_hz", 1000.0},
                   {"deviation_hz", 5000.0},
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

TEST_CASE("FM source zero deviation produces constant frequency", "[dsp][fm_source]") {
    FmSource src;
    src.configure({{"amplitude", 0.5},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"deviation_hz", 0.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    src.render_block(buf.data(), buf.size());

    // With deviation=0 and carrier=0, phase should be constant
    for (size_t i = 1; i < buf.size(); ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(buf[0].real(), 1e-6f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(buf[0].imag(), 1e-6f));
    }
}
