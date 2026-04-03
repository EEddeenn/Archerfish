#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/chirp_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Chirp start frequency matches f0", "[dsp][chirp]") {
    ChirpSource src;
    double f0 = 10e3;
    double f1 = 100e3;
    double fs = 1e6;
    double dur = 0.01;
    src.configure({{"amplitude", 1.0}, {"f0_hz", f0}, {"f1_hz", f1}, {"sample_rate", fs}, {"duration_sec", dur}});
    src.prepare();

    std::vector<std::complex<float>> buf(3);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 3);

    double dt = 1.0 / fs;
    double freq_slope = (f1 - f0) / dur;

    double t0 = 0.0;
    double phase0 = 2.0 * M_PI * (f0 * t0 + freq_slope * t0 * t0 / 2.0);
    REQUIRE_THAT(buf[0].real(), WithinAbs(static_cast<float>(std::cos(phase0)), 1e-5f));
    REQUIRE_THAT(buf[0].imag(), WithinAbs(static_cast<float>(std::sin(phase0)), 1e-5f));
}

TEST_CASE("Chirp end frequency approaches f1", "[dsp][chirp]") {
    ChirpSource src;
    double f0 = 10e3;
    double f1 = 100e3;
    double fs = 1e6;
    double dur = 0.001;
    size_t total_samples = static_cast<size_t>(fs * dur);
    src.configure({{"amplitude", 1.0}, {"f0_hz", f0}, {"f1_hz", f1}, {"sample_rate", fs}, {"duration_sec", dur}});
    src.prepare();

    std::vector<std::complex<float>> buf(total_samples);
    size_t n = src.render_block(buf.data(), total_samples);
    REQUIRE(n == total_samples);

    size_t last = total_samples - 1;
    double t_last = static_cast<double>(last) / fs;
    double freq_slope = (f1 - f0) / dur;
    double phase_last = 2.0 * M_PI * (f0 * t_last + freq_slope * t_last * t_last / 2.0);
    REQUIRE_THAT(buf[last].real(), WithinAbs(static_cast<float>(std::cos(phase_last)), 1e-4f));
}

TEST_CASE("Chirp amplitude is correct", "[dsp][chirp]") {
    ChirpSource src;
    src.configure({{"amplitude", 0.5}, {"f0_hz", 0}, {"f1_hz", 100e3}, {"sample_rate", 1e6}, {"duration_sec", 0.001}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    src.render_block(buf.data(), buf.size());

    for (const auto& s : buf) {
        float mag = std::abs(s);
        REQUIRE_THAT(mag, WithinAbs(0.5f, 1e-5f));
    }
}

TEST_CASE("Chirp duration is respected", "[dsp][chirp]") {
    ChirpSource src;
    src.configure({{"amplitude", 1.0}, {"f0_hz", 0}, {"f1_hz", 100e3}, {"sample_rate", 1000}, {"duration_sec", 0.01}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 10);

    n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 0);
}

TEST_CASE("Chirp metadata reports correct sample rate", "[dsp][chirp]") {
    ChirpSource src;
    src.configure({{"amplitude", 0.3}, {"f0_hz", 10e3}, {"f1_hz", 90e3}, {"sample_rate", 2e6}, {"duration_sec", 0.001}});
    src.prepare();

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.sample_rate, WithinAbs(2e6, 1e-9));
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(80e3, 1e-9));
}
