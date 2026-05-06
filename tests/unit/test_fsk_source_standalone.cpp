#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/fsk_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("FSK source produces constant amplitude", "[dsp][fsk_source]") {
    FskSource src;
    src.configure({{"amplitude", 0.5},
                   {"center_frequency_hz", 0.0},
                   {"symbol_rate", 1e3},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1e6},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(5000);
    src.render_block(buf.data(), buf.size());

    for (const auto& s : buf) {
        REQUIRE_THAT(std::abs(s), WithinAbs(0.5f, 0.01f));
    }
}

TEST_CASE("FSK source produces non-trivial output", "[dsp][fsk_source]") {
    FskSource src;
    src.configure({{"amplitude", 0.5},
                   {"center_frequency_hz", 100e3},
                   {"symbol_rate", 1e3},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1e6},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(1000);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 1000);

    bool has_variation = false;
    for (size_t i = 1; i < buf.size(); ++i) {
        if (buf[i] != buf[0]) has_variation = true;
    }
    REQUIRE(has_variation);
}

TEST_CASE("FSK source duration limit respected", "[dsp][fsk_source]") {
    FskSource src;
    src.configure({{"amplitude", 0.5},
                   {"center_frequency_hz", 0.0},
                   {"symbol_rate", 1e3},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1000},
                   {"duration_sec", 0.01},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("FSK source reset restarts generation", "[dsp][fsk_source]") {
    FskSource src;
    src.configure({{"amplitude", 0.5},
                   {"center_frequency_hz", 0.0},
                   {"symbol_rate", 1e3},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1e6},
                   {"duration_sec", 0.001},
                   {"seed", 42}});
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

TEST_CASE("FSK source metadata reports sample rate and amplitude", "[dsp][fsk_source]") {
    FskSource src;
    src.configure({{"amplitude", 0.3},
                   {"center_frequency_hz", 0.0},
                   {"symbol_rate", 1e3},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 2e6},
                   {"seed", 42}});
    src.prepare();

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.sample_rate, WithinAbs(2e6, 1e-9));
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.3, 1e-9));
    REQUIRE_THAT(meta.rms_amplitude, WithinAbs(0.3, 1e-9));
    REQUIRE_THAT(meta.crest_factor, WithinAbs(1.0, 1e-9));
}

TEST_CASE("FSK source rejects symbol rate above sample rate", "[dsp][fsk_source]") {
    FskSource src;
    src.configure({{"amplitude", 0.5},
                   {"center_frequency_hz", 0.0},
                   {"symbol_rate", 2000.0},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1000.0},
                   {"seed", 42}});

    CHECK_THROWS_AS(src.prepare(), std::invalid_argument);
}
