#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <limits>
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
    const double expected_rms = std::sqrt(0.3 * 0.3 + 0.2 * 0.2);
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(meta.rms_amplitude, WithinAbs(expected_rms, 1e-9));
    REQUIRE_THAT(meta.crest_factor, WithinAbs(0.5 / expected_rms, 1e-9));
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

TEST_CASE("Multi-tone rejects invalid tone definitions", "[dsp][multi_tone]") {
    MultiToneSource src;
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"tones", "bad"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({
                        {"sample_rate", 1e6},
                        {"tones", nlohmann::json::array({"bad"})}
                    }),
                    std::invalid_argument);
    CHECK_THROWS_AS(src.configure({
                        {"sample_rate", 1e6},
                        {"tones", nlohmann::json::array({{{"frequency_hz", "high"}, {"amplitude", 0.1}}})}
                    }),
                    std::invalid_argument);
    CHECK_THROWS_AS(src.configure({
                        {"sample_rate", 1e6},
                        {"tones", nlohmann::json::array({{{"frequency_hz", 100.0}, {"amplitude", "loud"}}})}
                    }),
                    std::invalid_argument);
    CHECK_THROWS_AS(src.configure({
                        {"sample_rate", 1e6},
                        {"tones", nlohmann::json::array({{{"frequency_hz", std::numeric_limits<double>::quiet_NaN()}, {"amplitude", 0.1}}})}
                    }),
                    std::invalid_argument);
    CHECK_THROWS_AS(src.configure({
                        {"sample_rate", 1e6},
                        {"tones", nlohmann::json::array({{{"frequency_hz", 100.0}, {"amplitude", -0.1}}})}
                    }),
                    std::invalid_argument);
    CHECK_THROWS_AS(src.configure({
                        {"sample_rate", 1e6},
                        {"tones", nlohmann::json::array({{{"frequency_hz", 100.0},
                                                          {"amplitude", static_cast<double>(std::numeric_limits<float>::max()) * 2.0}}})}
                    }),
                    std::out_of_range);
    CHECK_THROWS_AS(src.configure({
                        {"sample_rate", 1e6},
                        {"tones", nlohmann::json::array({
                            {{"frequency_hz", 0.0}, {"amplitude", static_cast<double>(std::numeric_limits<float>::max()) * 0.75}},
                            {{"frequency_hz", 0.0}, {"amplitude", static_cast<double>(std::numeric_limits<float>::max()) * 0.75}}
                        })}
                    }),
                    std::out_of_range);
}

TEST_CASE("Multi-tone keeps prior configuration after invalid reconfigure", "[dsp][multi_tone]") {
    MultiToneSource src;
    src.configure({
        {"sample_rate", 1e6},
        {"tones", nlohmann::json::array({
            {{"frequency_hz", 0.0}, {"amplitude", 0.3}},
            {{"frequency_hz", 1000.0}, {"amplitude", 0.2}}
        })}
    });

    CHECK_THROWS_AS(src.configure({
                        {"sample_rate", 2e6},
                        {"tones", nlohmann::json::array({
                            {{"frequency_hz", 0.0}, {"amplitude", 0.9}},
                            {{"frequency_hz", std::numeric_limits<double>::quiet_NaN()}, {"amplitude", 0.1}}
                        })}
                    }),
                    std::invalid_argument);

    auto meta = src.report_metadata();
    const double expected_rms = std::sqrt(0.3 * 0.3 + 0.2 * 0.2);
    REQUIRE_THAT(meta.sample_rate, WithinAbs(1e6, 1e-9));
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(meta.rms_amplitude, WithinAbs(expected_rms, 1e-9));
    REQUIRE_THAT(meta.crest_factor, WithinAbs(0.5 / expected_rms, 1e-9));
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(1000.0, 1e-9));
}

TEST_CASE("Multi-tone render validates output buffer", "[dsp][multi_tone]") {
    MultiToneSource src;
    src.configure({
        {"sample_rate", 1e6},
        {"tones", nlohmann::json::array({{{"frequency_hz", 0.0}, {"amplitude", 0.2}}})}
    });
    src.prepare();

    CHECK(src.render_block(nullptr, 0) == 0);
    CHECK_THROWS_AS(src.render_block(nullptr, 1), std::invalid_argument);
}
