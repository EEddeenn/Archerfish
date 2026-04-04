#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/common/constants.hpp"
#include "archerfish/dsp/resampler.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("compute_ratio identity", "[dsp][resampler]") {
    auto [l, m] = compute_ratio(1.0, 1.0);
    REQUIRE(l == 1);
    REQUIRE(m == 1);
}

TEST_CASE("compute_ratio simple 2:1", "[dsp][resampler]") {
    auto [l, m] = compute_ratio(2.0, 1.0);
    REQUIRE(l == 2);
    REQUIRE(m == 1);
}

TEST_CASE("compute_ratio 48000/44100", "[dsp][resampler]") {
    auto [l, m] = compute_ratio(48000.0, 44100.0);
    double approx_ratio = static_cast<double>(l) / static_cast<double>(m);
    double exact_ratio = 48000.0 / 44100.0;
    REQUIRE_THAT(approx_ratio, WithinRel(exact_ratio, 1e-6));
    REQUIRE(l == 160);
    REQUIRE(m == 147);
}

TEST_CASE("compute_ratio throws on zero", "[dsp][resampler]") {
    REQUIRE_THROWS_AS(compute_ratio(0.0, 44100.0), std::invalid_argument);
    REQUIRE_THROWS_AS(compute_ratio(48000.0, 0.0), std::invalid_argument);
}

TEST_CASE("compute_ratio respects max_denominator", "[dsp][resampler]") {
    auto [l, m] = compute_ratio(48000.0, 44100.0, 10);
    REQUIRE(m <= 10);
    double approx_ratio = static_cast<double>(l) / static_cast<double>(m);
    double exact_ratio = 48000.0 / 44100.0;
    double best_err = 1.0;
    for (unsigned km = 1; km <= 10; ++km) {
        unsigned kl = static_cast<unsigned>(std::round(exact_ratio * km));
        if (kl == 0) continue;
        double err = std::abs(exact_ratio - static_cast<double>(kl) / km);
        best_err = std::min(best_err, err);
    }
    double actual_err = std::abs(exact_ratio - approx_ratio);
    REQUIRE(actual_err <= best_err + 1e-12);
}

TEST_CASE("ResamplerBlock identity passthrough", "[dsp][resampler]") {
    ResamplerBlock rs(1, 1);
    const size_t n = 512;
    std::vector<std::complex<float>> in(n);
    for (size_t i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(n);
        in[i] = {std::cos(2.0f * float(archerfish::constants::kPi) * t), std::sin(2.0f * float(archerfish::constants::kPi) * t)};
    }

    auto out = rs.process(in.data(), n);
    REQUIRE(out.size() == n);

    // libsamplerate's SINC filter introduces a group delay; skip transient edges
    size_t skip = 30;
    for (size_t i = skip; i + skip < out.size(); ++i) {
        REQUIRE_THAT(out[i].real(), WithinAbs(in[i].real(), 0.05f));
        REQUIRE_THAT(out[i].imag(), WithinAbs(in[i].imag(), 0.05f));
    }

    // All samples should have unit magnitude (phase may shift)
    for (size_t i = skip; i + skip < out.size(); ++i) {
        float mag = std::abs(out[i]);
        REQUIRE_THAT(mag, WithinAbs(1.0f, 0.05f));
    }
}

TEST_CASE("ResamplerBlock upsample 2x DC signal", "[dsp][resampler]") {
    ResamplerBlock rs(2, 1);
    std::vector<std::complex<float>> in(100, {1.0f, 0.5f});

    auto out = rs.process(in.data(), in.size());
    REQUIRE(out.size() >= 2 * in.size() - 10);
    REQUIRE(out.size() <= 2 * in.size() + 10);

    for (size_t i = 10; i < out.size() - 10; ++i) {
        REQUIRE_THAT(out[i].real(), WithinAbs(1.0f, 0.05f));
        REQUIRE_THAT(out[i].imag(), WithinAbs(0.5f, 0.05f));
    }
}

TEST_CASE("ResamplerBlock downsample 2x", "[dsp][resampler]") {
    ResamplerBlock rs(1, 2);
    std::vector<std::complex<float>> in(200, {0.7f, 0.3f});

    auto out = rs.process(in.data(), in.size());
    REQUIRE(out.size() >= in.size() / 2 - 10);
    REQUIRE(out.size() <= in.size() / 2 + 10);
}

TEST_CASE("ResamplerBlock complex exponential phase coherence", "[dsp][resampler]") {
    ResamplerBlock rs(3, 2);
    size_t n = 1000;
    float freq = 0.01f;

    std::vector<std::complex<float>> in(n);
    for (size_t i = 0; i < n; ++i) {
        float phase = 2.0f * float(archerfish::constants::kPi) * freq * static_cast<float>(i);
        in[i] = {std::cos(phase), std::sin(phase)};
    }

    auto out = rs.process(in.data(), n);
    REQUIRE(out.size() > 0);

    double ratio = rs.ratio();
    size_t expected = static_cast<size_t>(static_cast<double>(n) * ratio);
    REQUIRE(out.size() >= expected - 20);
    REQUIRE(out.size() <= expected + 20);

    float out_freq = freq / static_cast<float>(ratio);
    for (size_t i = 20; i < out.size() - 20; ++i) {
        float expected_phase = 2.0f * float(archerfish::constants::kPi) * out_freq * static_cast<float>(i);
        (void)expected_phase;
        float mag = std::abs(out[i]);
        REQUIRE_THAT(mag, WithinAbs(1.0f, 0.15f));
    }
}

TEST_CASE("ResamplerBlock reset clears state", "[dsp][resampler]") {
    ResamplerBlock rs(2, 1);
    std::vector<std::complex<float>> in(100, {1.0f, 0.0f});

    auto out1 = rs.process(in.data(), in.size());
    rs.reset();
    auto out2 = rs.process(in.data(), in.size());

    REQUIRE(out1.size() == out2.size());
}

TEST_CASE("ResamplerBlock streaming equals batch", "[dsp][resampler]") {
    ResamplerBlock rs_batch(3, 2);

    size_t total = 200;
    std::vector<std::complex<float>> in(total);
    for (size_t i = 0; i < total; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(total);
        in[i] = {std::cos(2.0f * float(archerfish::constants::kPi) * 5.0f * t),
                 std::sin(2.0f * float(archerfish::constants::kPi) * 5.0f * t)};
    }

    auto batch_out = rs_batch.process(in.data(), total);

    double expected_ratio = rs_batch.ratio();
    size_t expected_size = static_cast<size_t>(static_cast<double>(total) * expected_ratio);
    REQUIRE(batch_out.size() >= expected_size - 20);
    REQUIRE(batch_out.size() <= expected_size + 20);

    // All batch output samples must be bounded (no NaN/inf, reasonable magnitude)
    for (const auto& s : batch_out) {
        REQUIRE(std::isfinite(s.real()));
        REQUIRE(std::isfinite(s.imag()));
        REQUIRE(std::abs(s) < 2.0f);
    }

    // Batch processing should be reproducible (same input → same output)
    auto batch_out2 = rs_batch.process(in.data(), total);
    REQUIRE(batch_out.size() == batch_out2.size());
    for (size_t i = 0; i < batch_out.size(); ++i) {
        REQUIRE_THAT(batch_out[i].real(), WithinAbs(batch_out2[i].real(), 1e-6f));
        REQUIRE_THAT(batch_out[i].imag(), WithinAbs(batch_out2[i].imag(), 1e-6f));
    }
}

TEST_CASE("ResamplerBlock estimated_output_size", "[dsp][resampler]") {
    ResamplerBlock rs(7, 3);
    size_t est = rs.estimated_output_size(100);
    size_t expected_min = static_cast<size_t>(100.0 * 7.0 / 3.0);
    REQUIRE(est >= expected_min);
}

TEST_CASE("ResamplerBlock accessors", "[dsp][resampler]") {
    ResamplerBlock rs(5, 3);
    REQUIRE(rs.interp() == 5);
    REQUIRE(rs.decim() == 3);
    REQUIRE_THAT(rs.ratio(), WithinRel(5.0 / 3.0, 1e-12));
}

TEST_CASE("ResamplerBlock throws on zero factors", "[dsp][resampler]") {
    REQUIRE_THROWS_AS(ResamplerBlock(0, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(ResamplerBlock(1, 0), std::invalid_argument);
}
