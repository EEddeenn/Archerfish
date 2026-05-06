#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/noise_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Noise source has approximately correct RMS power", "[dsp][noise][statistics]") {
    NoiseSource src;
    double amp = 0.1;
    src.configure({{"amplitude", amp}, {"sample_rate", 1e6}, {"seed", 42}});
    src.prepare();

    const size_t N = 100000;
    std::vector<std::complex<float>> buf(N);
    src.render_block(buf.data(), buf.size());

    double sum_sq = 0.0;
    for (const auto& s : buf) {
        sum_sq += static_cast<double>(s.real()) * static_cast<double>(s.real());
        sum_sq += static_cast<double>(s.imag()) * static_cast<double>(s.imag());
    }
    double rms = std::sqrt(sum_sq / static_cast<double>(2 * N));

    REQUIRE_THAT(rms, WithinAbs(amp, amp * 0.15));
}

TEST_CASE("Noise source I and Q are uncorrelated", "[dsp][noise][statistics]") {
    NoiseSource src;
    src.configure({{"amplitude", 0.5}, {"sample_rate", 1e6}, {"seed", 42}});
    src.prepare();

    const size_t N = 50000;
    std::vector<std::complex<float>> buf(N);
    src.render_block(buf.data(), buf.size());

    double cross_corr = 0.0;
    for (const auto& s : buf) {
        cross_corr += static_cast<double>(s.real()) * static_cast<double>(s.imag());
    }
    cross_corr /= static_cast<double>(N);

    REQUIRE(std::abs(cross_corr) < 0.01);
}

TEST_CASE("Noise source with fixed seed is reproducible", "[dsp][noise][statistics]") {
    NoiseSource src1;
    src1.configure({{"amplitude", 0.3}, {"sample_rate", 1e6}, {"seed", 123}});
    src1.prepare();

    NoiseSource src2;
    src2.configure({{"amplitude", 0.3}, {"sample_rate", 1e6}, {"seed", 123}});
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

TEST_CASE("Noise source has zero mean", "[dsp][noise][statistics]") {
    NoiseSource src;
    src.configure({{"amplitude", 0.5}, {"sample_rate", 1e6}, {"seed", 42}});
    src.prepare();

    const size_t N = 100000;
    std::vector<std::complex<float>> buf(N);
    src.render_block(buf.data(), buf.size());

    double mean_re = 0.0, mean_im = 0.0;
    for (const auto& s : buf) {
        mean_re += static_cast<double>(s.real());
        mean_im += static_cast<double>(s.imag());
    }
    mean_re /= static_cast<double>(N);
    mean_im /= static_cast<double>(N);

    REQUIRE(std::abs(mean_re) < 0.005);
    REQUIRE(std::abs(mean_im) < 0.005);
}

TEST_CASE("Noise source metadata is consistent", "[dsp][noise][statistics]") {
    NoiseSource src;
    double amp = 0.4;
    src.configure({{"amplitude", amp}, {"sample_rate", 2e6}, {"seed", 42}});
    src.prepare();

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(amp * 4.0, 1e-9));
    REQUIRE_THAT(meta.sample_rate, WithinAbs(2e6, 1e-9));
    REQUIRE(meta.repeats);
}

TEST_CASE("Noise source render validates output buffer", "[dsp][noise][statistics]") {
    NoiseSource src;
    src.configure({{"amplitude", 0.1}, {"sample_rate", 1e6}, {"seed", 42}});
    src.prepare();

    CHECK(src.render_block(nullptr, 0) == 0);
    CHECK_THROWS_AS(src.render_block(nullptr, 1), std::invalid_argument);
}
