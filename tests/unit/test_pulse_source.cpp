#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#include "archerfish/common/constants.hpp"
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
    (void)static_cast<size_t>(std::round(pw * fs));

    std::vector<std::complex<float>> buf1(pri_samples);
    std::vector<std::complex<float>> buf2(pri_samples);

    src.render_block(buf1.data(), buf1.size());
    src.render_block(buf2.data(), buf2.size());

    float phase_at_start_of_pulse2 = std::arg(buf2[0]);
    double expected_phase = 2.0 * archerfish::constants::kPi * freq / fs * static_cast<double>(pri_samples);
    expected_phase = std::fmod(expected_phase, 2.0 * archerfish::constants::kPi);
    if (expected_phase > archerfish::constants::kPi) expected_phase -= 2.0 * archerfish::constants::kPi;

    float diff = std::abs(phase_at_start_of_pulse2 - static_cast<float>(expected_phase));
    if (diff > static_cast<float>(archerfish::constants::kPi))
        diff = 2.0f * static_cast<float>(archerfish::constants::kPi) - diff;
    REQUIRE_THAT(diff, WithinAbs(0.0f, 0.01f));
}

TEST_CASE("PulseSource wraps high carrier phase modulo one cycle", "[dsp][pulse]") {
    PulseSource src;
    src.configure({{"amplitude", 1.0},
                   {"frequency_hz", 2.5e6},
                   {"sample_rate", 1e6},
                   {"pulse_width_sec", 4e-6},
                   {"pri_sec", 4e-6},
                   {"mode", "train"}});
    src.prepare();

    std::vector<std::complex<float>> buf(4);
    REQUIRE(src.render_block(buf.data(), buf.size()) == buf.size());

    REQUIRE_THAT(buf[0].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(buf[1].real(), WithinAbs(-1.0f, 1e-5f));
    REQUIRE_THAT(buf[2].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(buf[0].imag(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(buf[1].imag(), WithinAbs(0.0f, 1e-5f));
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

TEST_CASE("PulseSource rejects invalid timing and mode", "[dsp][pulse]") {
    PulseSource src;
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"frequency_hz", "high"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"pulse_width_sec", "wide"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"pri_sec", "slow"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"mode", false}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"frequency_hz", std::numeric_limits<double>::quiet_NaN()}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"pulse_width_sec", 0.0}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"pri_sec", 0.0}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"pulse_width_sec", 10e-6}, {"pri_sec", 1e-6}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"mode", "burst"}}), std::invalid_argument);
}

TEST_CASE("PulseSource keeps prior configuration after invalid reconfigure", "[dsp][pulse]") {
    PulseSource src;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", 1e6},
                   {"pulse_width_sec", 1e-6},
                   {"pri_sec", 10e-6},
                   {"mode", "train"}});

    CHECK_THROWS_AS(src.configure({{"amplitude", 0.8},
                                   {"frequency_hz", 10e3},
                                   {"sample_rate", 2e6},
                                   {"pulse_width_sec", 20e-6},
                                   {"pri_sec", 10e-6},
                                   {"mode", "single"}}),
                    std::invalid_argument);

    src.prepare();
    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.sample_rate, WithinAbs(1e6, 1e-9));
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.2, 1e-9));
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(2e6, 1.0));
    REQUIRE(meta.repeats);
}

TEST_CASE("PulseSource prepare rejects timing below sample resolution", "[dsp][pulse]") {
    PulseSource src;
    src.configure({{"sample_rate", 1e6}, {"pulse_width_sec", 0.1e-6}, {"pri_sec", 1e-6}});
    CHECK_THROWS_AS(src.prepare(), std::invalid_argument);
}

TEST_CASE("PulseSource render validates output buffer", "[dsp][pulse]") {
    PulseSource src;
    src.configure({{"sample_rate", 1e6}, {"pulse_width_sec", 1e-6}, {"pri_sec", 10e-6}});
    src.prepare();

    CHECK(src.render_block(nullptr, 0) == 0);
    CHECK_THROWS_AS(src.render_block(nullptr, 1), std::invalid_argument);
}

TEST_CASE("PulseSource reconfigure invalidates prepared timing", "[dsp][pulse]") {
    PulseSource src;
    src.configure({{"sample_rate", 1e6}, {"pulse_width_sec", 1e-6}, {"pri_sec", 10e-6}});
    src.prepare();

    std::vector<std::complex<float>> buf(2);
    REQUIRE(src.render_block(buf.data(), buf.size()) == 2);

    src.configure({{"sample_rate", 1e6}, {"pulse_width_sec", 2e-6}, {"pri_sec", 10e-6}});
    CHECK_THROWS_AS(src.render_block(buf.data(), buf.size()), std::logic_error);

    src.prepare();
    REQUIRE(src.render_block(buf.data(), buf.size()) == 2);
}

TEST_CASE("PulseSource prepare rejects timing sample count overflow", "[dsp][pulse]") {
    PulseSource src;
    src.configure({{"sample_rate", 1e308}, {"pulse_width_sec", 1e308}, {"pri_sec", 1e308}});
    CHECK_THROWS_AS(src.prepare(), std::overflow_error);
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
