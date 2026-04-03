#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/multi_tone_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Single tone matches CW behavior", "[dsp][multi_tone]") {
    MultiToneSource src;
    src.configure({
        {"sample_rate", 1e6},
        {"tones", nlohmann::json::array({{{"frequency_hz", 0.0}, {"amplitude", 0.2}}})}
    });
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(0.2f, 1e-6f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("Multiple tones produce correct peak amplitude", "[dsp][multi_tone]") {
    MultiToneSource src;
    src.configure({
        {"sample_rate", 1e6},
        {"tones", nlohmann::json::array({
            {{"frequency_hz", 0.0}, {"amplitude", 0.3}},
            {{"frequency_hz", 0.0}, {"amplitude", 0.2}}
        })}
    });
    src.prepare();

    std::vector<std::complex<float>> buf(10);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 10);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(0.5f, 1e-6f));
    }

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5, 1e-9));
}

TEST_CASE("Multi-tone duration respected", "[dsp][multi_tone]") {
    MultiToneSource src;
    src.configure({
        {"sample_rate", 1000},
        {"duration_sec", 0.01},
        {"tones", nlohmann::json::array({{{"frequency_hz", 100.0}, {"amplitude", 0.1}}})}
    });
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 10);

    n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 0);
}
