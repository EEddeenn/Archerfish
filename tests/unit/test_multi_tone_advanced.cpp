#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/multi_tone_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Multi-tone with many tones produces output", "[dsp][multi_tone][advanced]") {
    MultiToneSource src;
    nlohmann::json tones = nlohmann::json::array();
    for (int i = 0; i < 10; ++i) {
        tones.push_back({{"frequency_hz", 100e3 * i}, {"amplitude", 0.01}});
    }
    src.configure({{"amplitude", 0.1}, {"sample_rate", 10e6}, {"tones", tones}});
    src.prepare();

    std::vector<std::complex<float>> buf(1000);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 1000);

    bool has_nonzero = false;
    for (const auto& s : buf) {
        if (std::abs(s) > 1e-6f) has_nonzero = true;
    }
    REQUIRE(has_nonzero);
}

TEST_CASE("Multi-tone single tone matches CW", "[dsp][multi_tone][advanced]") {
    MultiToneSource src;
    nlohmann::json tones = nlohmann::json::array();
    tones.push_back({{"frequency_hz", 0.0}, {"amplitude", 0.2}});
    src.configure({{"amplitude", 1.0}, {"sample_rate", 1e6}, {"tones", tones}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(0.2f, 1e-4f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(0.0f, 1e-4f));
    }
}

TEST_CASE("Multi-tone reset produces identical output", "[dsp][multi_tone][advanced]") {
    MultiToneSource src;
    nlohmann::json tones = nlohmann::json::array();
    tones.push_back({{"frequency_hz", 100e3}, {"amplitude", 0.1}});
    tones.push_back({{"frequency_hz", 200e3}, {"amplitude", 0.1}});
    src.configure({{"amplitude", 0.2}, {"sample_rate", 1e6}, {"tones", tones}, {"duration_sec", 0.001}});
    src.prepare();

    std::vector<std::complex<float>> buf1(1000);
    src.render_block(buf1.data(), buf1.size());

    src.reset();

    std::vector<std::complex<float>> buf2(1000);
    size_t n = src.render_block(buf2.data(), buf2.size());
    REQUIRE(n == 1000);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf2[i].real(), WithinAbs(buf1[i].real(), 1e-6f));
    }
}

TEST_CASE("Multi-tone duration limit respected", "[dsp][multi_tone][advanced]") {
    MultiToneSource src;
    nlohmann::json tones = nlohmann::json::array();
    tones.push_back({{"frequency_hz", 100e3}, {"amplitude", 0.1}});
    src.configure({{"amplitude", 0.1}, {"sample_rate", 1000}, {"tones", tones}, {"duration_sec", 0.01}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("Multi-tone zero frequency tones produce DC component", "[dsp][multi_tone][advanced]") {
    MultiToneSource src;
    nlohmann::json tones = nlohmann::json::array();
    tones.push_back({{"frequency_hz", 0.0}, {"amplitude", 0.5}});
    src.configure({{"amplitude", 1.0}, {"sample_rate", 1e6}, {"tones", tones}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    src.render_block(buf.data(), buf.size());

    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(buf[0].real(), 1e-6f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(0.0f, 1e-6f));
    }
}
