#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/noise_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Noise mean approximately zero for large sample count", "[dsp][noise]") {
    NoiseSource src;
    src.configure({{"amplitude", 1.0}, {"sample_rate", 1e6}, {"seed", 123}});
    src.prepare();

    size_t N = 100000;
    std::vector<std::complex<float>> buf(N);
    size_t n = src.render_block(buf.data(), N);
    REQUIRE(n == N);

    double sum_re = 0.0, sum_im = 0.0;
    for (const auto& s : buf) {
        sum_re += static_cast<double>(s.real());
        sum_im += static_cast<double>(s.imag());
    }
    double mean_re = sum_re / static_cast<double>(N);
    double mean_im = sum_im / static_cast<double>(N);

    REQUIRE_THAT(mean_re, WithinAbs(0.0, 0.01));
    REQUIRE_THAT(mean_im, WithinAbs(0.0, 0.01));
}

TEST_CASE("Noise variance approximately amplitude^2/2 per component", "[dsp][noise]") {
    NoiseSource src;
    double amp = 0.5;
    src.configure({{"amplitude", amp}, {"sample_rate", 1e6}, {"seed", 42}});
    src.prepare();

    size_t N = 100000;
    std::vector<std::complex<float>> buf(N);
    src.render_block(buf.data(), N);

    double sum_sq_re = 0.0, sum_sq_im = 0.0;
    for (const auto& s : buf) {
        sum_sq_re += static_cast<double>(s.real()) * static_cast<double>(s.real());
        sum_sq_im += static_cast<double>(s.imag()) * static_cast<double>(s.imag());
    }
    double var_re = sum_sq_re / static_cast<double>(N);
    double var_im = sum_sq_im / static_cast<double>(N);
    double expected_var = amp * amp;

    REQUIRE_THAT(var_re, WithinAbs(expected_var, expected_var * 0.1));
    REQUIRE_THAT(var_im, WithinAbs(expected_var, expected_var * 0.1));
}

TEST_CASE("Noise seeded reproducibility", "[dsp][noise]") {
    NoiseSource src1;
    src1.configure({{"amplitude", 1.0}, {"sample_rate", 1e6}, {"seed", 42}});
    src1.prepare();

    NoiseSource src2;
    src2.configure({{"amplitude", 1.0}, {"sample_rate", 1e6}, {"seed", 42}});
    src2.prepare();

    std::vector<std::complex<float>> buf1(1000);
    std::vector<std::complex<float>> buf2(1000);
    src1.render_block(buf1.data(), buf1.size());
    src2.render_block(buf2.data(), buf2.size());

    for (size_t i = 0; i < buf1.size(); ++i) {
        REQUIRE(buf1[i].real() == buf2[i].real());
        REQUIRE(buf1[i].imag() == buf2[i].imag());
    }
}

TEST_CASE("Noise duration limit respected", "[dsp][noise]") {
    NoiseSource src;
    src.configure({{"amplitude", 1.0}, {"sample_rate", 1000}, {"duration_sec", 0.01}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 10);

    n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 0);
}
