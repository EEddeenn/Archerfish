#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/resampler.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Resampler 1:1 identity preserves samples", "[dsp][resampler][ratios]") {
    ResamplerBlock resampler(1, 1);

    std::vector<std::complex<float>> in(100);
    for (size_t i = 0; i < in.size(); ++i) {
        in[i] = {static_cast<float>(i) * 0.01f, static_cast<float>(i) * -0.01f};
    }

    auto out = resampler.process(in.data(), in.size());
    REQUIRE(out.size() >= 90);

    for (size_t i = 10; i + 10 < std::min(out.size(), in.size()); ++i) {
        REQUIRE_THAT(out[i].real(), WithinAbs(in[i].real(), 0.02f));
    }
}

TEST_CASE("Resampler 2:1 downsampling produces fewer samples", "[dsp][resampler][ratios]") {
    ResamplerBlock resampler(1, 2);

    std::vector<std::complex<float>> in(1000, {1.0f, 0.0f});
    auto out = resampler.process(in.data(), in.size());

    REQUIRE(out.size() < in.size());
    REQUIRE(out.size() > 0);
}

TEST_CASE("Resampler 1:2 upsampling produces more samples", "[dsp][resampler][ratios]") {
    ResamplerBlock resampler(2, 1);

    std::vector<std::complex<float>> in(1000, {1.0f, 0.0f});
    auto out = resampler.process(in.data(), in.size());

    REQUIRE(out.size() > in.size());
}

TEST_CASE("Resampler 3:2 rational ratio", "[dsp][resampler][ratios]") {
    ResamplerBlock resampler(3, 2);

    std::vector<std::complex<float>> in(1000);
    for (size_t i = 0; i < in.size(); ++i) {
        float phase = 2.0f * static_cast<float>(M_PI) * 0.01f * static_cast<float>(i);
        in[i] = {std::cos(phase), std::sin(phase)};
    }

    auto out = resampler.process(in.data(), in.size());
    REQUIRE(out.size() > 0);
    REQUIRE_THAT(resampler.ratio(), WithinAbs(1.5, 1e-9));
}

TEST_CASE("Resampler reset clears state", "[dsp][resampler][ratios]") {
    ResamplerBlock resampler(2, 1);

    std::vector<std::complex<float>> in(100, {1.0f, 0.0f});
    auto out1 = resampler.process(in.data(), in.size());

    resampler.reset();

    auto out2 = resampler.process(in.data(), in.size());
    REQUIRE(out1.size() == out2.size());
}

TEST_CASE("compute_ratio returns exact for simple ratios", "[dsp][resampler][ratios]") {
    auto [l, m] = compute_ratio(2.0, 1.0);
    REQUIRE(l == 2);
    REQUIRE(m == 1);
}
