#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/ofdm_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("OFDM with 64-point FFT produces output", "[dsp][ofdm][parameters]") {
    OfdmSource src;
    src.configure({{"amplitude", 0.15}, {"sample_rate", 20e6},
                   {"fft_size", 64}, {"cyclic_prefix_size", 16}, {"active_subcarriers", 60},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(1000);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n > 0);
}

TEST_CASE("OFDM with 128-point FFT produces output", "[dsp][ofdm][parameters]") {
    OfdmSource src;
    src.configure({{"amplitude", 0.1}, {"sample_rate", 20e6},
                   {"fft_size", 128}, {"cyclic_prefix_size", 32}, {"active_subcarriers", 120},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(2000);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n > 0);
}

TEST_CASE("OFDM with 256-point FFT produces output", "[dsp][ofdm][parameters]") {
    OfdmSource src;
    src.configure({{"amplitude", 0.1}, {"sample_rate", 40e6},
                   {"fft_size", 256}, {"cyclic_prefix_size", 64}, {"active_subcarriers", 240},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(4000);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n > 0);
}

TEST_CASE("OFDM duration limit respected", "[dsp][ofdm][parameters]") {
    OfdmSource src;
    src.configure({{"amplitude", 0.1}, {"sample_rate", 1000},
                   {"fft_size", 64}, {"cyclic_prefix_size", 16}, {"active_subcarriers", 60},
                   {"duration_sec", 0.01}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("OFDM reset produces identical output", "[dsp][ofdm][parameters]") {
    OfdmSource src;
    src.configure({{"amplitude", 0.1}, {"sample_rate", 1e6},
                   {"fft_size", 64}, {"cyclic_prefix_size", 16}, {"active_subcarriers", 60},
                   {"duration_sec", 0.001}, {"seed", 42}});
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
