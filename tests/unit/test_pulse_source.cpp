#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/pulse_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("PulseSource single mode produces exactly one pulse", "[dsp][pulse]") {
    PulseSource src;
    double fs = 1e6;
    double pw = 1e-6;
    double pri = 10e-6;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"pulse_width_sec", pw},
                   {"pri_sec", pri},
                   {"mode", "single"}});
    src.prepare();

    size_t pri_samples = static_cast<size_t>(std::round(pri * fs));
    size_t pw_samples = static_cast<size_t>(std::round(pw * fs));

    std::vector<std::complex<float>> buf(pri_samples);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == pri_samples);

    size_t nonzero = 0;
    for (size_t i = 0; i < n; ++i) {
        float mag = std::abs(buf[i]);
        if (mag > 1e-6f) {
            nonzero++;
            REQUIRE_THAT(mag, WithinAbs(0.2f, 1e-5f));
        }
    }
    REQUIRE(nonzero == pw_samples);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("PulseSource train mode produces repeating ON/OFF pattern", "[dsp][pulse]") {
    PulseSource src;
    double fs = 1e6;
    double pw = 1e-6;
    double pri = 5e-6;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"pulse_width_sec", pw},
                   {"pri_sec", pri},
                   {"mode", "train"}});
    src.prepare();

    size_t pw_samples = static_cast<size_t>(std::round(pw * fs));
    size_t pri_samples = static_cast<size_t>(std::round(pri * fs));

    size_t total = pri_samples * 3;
    std::vector<std::complex<float>> buf(total);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == total);

    for (size_t pulse = 0; pulse < 3; ++pulse) {
        for (size_t i = 0; i < pri_samples; ++i) {
            size_t idx = pulse * pri_samples + i;
            if (i < pw_samples) {
                REQUIRE_THAT(std::abs(buf[idx]), WithinAbs(0.2f, 1e-5f));
            } else {
                REQUIRE_THAT(std::abs(buf[idx]), WithinAbs(0.0f, 1e-6f));
            }
        }
    }
}

TEST_CASE("PulseSource phase advances during OFF periods", "[dsp][pulse]") {
    PulseSource src;
    double fs = 1e6;
    double freq = 100e3;
    double pw = 1e-6;
    double pri = 5e-6;
    src.configure({{"amplitude", 1.0},
                   {"frequency_hz", freq},
                   {"sample_rate", fs},
                   {"pulse_width_sec", pw},
                   {"pri_sec", pri},
                   {"mode", "train"}});
    src.prepare();

    size_t pri_samples = static_cast<size_t>(std::round(pri * fs));
    size_t pw_samples = static_cast<size_t>(std::round(pw * fs));

    std::vector<std::complex<float>> buf1(pri_samples);
    std::vector<std::complex<float>> buf2(pri_samples);

    src.render_block(buf1.data(), buf1.size());
    src.render_block(buf2.data(), buf2.size());

    float phase_at_start_of_pulse2 = std::arg(buf2[0]);
    double expected_phase = 2.0 * M_PI * freq / fs * static_cast<double>(pri_samples);
    expected_phase = std::fmod(expected_phase, 2.0 * M_PI);
    if (expected_phase > M_PI) expected_phase -= 2.0 * M_PI;

    float diff = std::abs(phase_at_start_of_pulse2 - static_cast<float>(expected_phase));
    if (diff > static_cast<float>(M_PI))
        diff = 2.0f * static_cast<float>(M_PI) - diff;
    REQUIRE_THAT(diff, WithinAbs(0.0f, 0.01f));
}

TEST_CASE("PulseSource zero frequency produces constant amplitude during ON", "[dsp][pulse]") {
    PulseSource src;
    double fs = 1e6;
    double pw = 10e-6;
    double pri = 10e-6;
    src.configure({{"amplitude", 0.5},
                   {"frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"pulse_width_sec", pw},
                   {"pri_sec", pri},
                   {"mode", "train"}});
    src.prepare();

    size_t pw_samples = static_cast<size_t>(std::round(pw * fs));
    std::vector<std::complex<float>> buf(pw_samples);
    src.render_block(buf.data(), buf.size());

    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(0.5f, 1e-5f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(0.0f, 1e-5f));
    }
}

TEST_CASE("PulseSource duration limits total output", "[dsp][pulse]") {
    PulseSource src;
    double fs = 1e6;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"pulse_width_sec", 1e-6},
                   {"pri_sec", 10e-6},
                   {"mode", "train"},
                   {"duration_sec", 0.00002}});
    src.prepare();

    size_t total = static_cast<size_t>(std::round(0.00002 * fs));
    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == total);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("PulseSource metadata is correct", "[dsp][pulse]") {
    PulseSource src;
    src.configure({{"amplitude", 0.2},
                   {"sample_rate", 1e6},
                   {"pulse_width_sec", 1e-6},
                   {"pri_sec", 10e-6},
                   {"mode", "train"}});
    src.prepare();

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(2e6, 1.0));
    REQUIRE(meta.repeats == true);
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.2, 1e-9));
}

TEST_CASE("PulseSource reset restarts generation", "[dsp][pulse]") {
    PulseSource src;
    double fs = 1e6;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"pulse_width_sec", 1e-6},
                   {"pri_sec", 10e-6},
                   {"mode", "single"}});
    src.prepare();

    std::vector<std::complex<float>> buf(10);
    src.render_block(buf.data(), buf.size());

    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 0);

    src.reset();

    n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 10);
}
