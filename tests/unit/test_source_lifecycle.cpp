#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <memory>
#include <vector>

#include "archerfish/dsp/cw_source.hpp"
#include "archerfish/dsp/chirp_source.hpp"
#include "archerfish/dsp/noise_source.hpp"
#include "archerfish/dsp/source_factory.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("Source lifecycle: create, configure, render, reset", "[dsp][lifecycle]") {
    auto src = create_source(WaveformType::CW);
    REQUIRE(src != nullptr);

    src->configure({{"amplitude", 0.2}, {"frequency_hz", 100e3}, {"sample_rate", 1e6}});
    src->prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src->render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    src->reset();

    size_t n2 = src->render_block(buf.data(), buf.size());
    REQUIRE(n2 == 100);
}

TEST_CASE("Source lifecycle: multiple render blocks", "[dsp][lifecycle]") {
    CwSource src;
    src.configure({{"amplitude", 0.3}, {"frequency_hz", 50e3}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(50);

    for (int block = 0; block < 10; ++block) {
        size_t n = src.render_block(buf.data(), buf.size());
        REQUIRE(n == 50);
    }
}

TEST_CASE("CW source keeps prior configuration after invalid reconfigure", "[dsp][lifecycle]") {
    CwSource src;
    src.configure({{"amplitude", 0.2}, {"frequency_hz", 100e3}, {"sample_rate", 1e6}});

    CHECK_THROWS_AS(src.configure({
                        {"amplitude", 0.8},
                        {"frequency_hz", std::numeric_limits<double>::quiet_NaN()},
                        {"sample_rate", 2e6},
                    }),
                    std::invalid_argument);

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.2, 1e-9));
    REQUIRE_THAT(meta.sample_rate, WithinAbs(1e6, 1e-9));

    src.prepare();
    std::vector<std::complex<float>> buf(2);
    REQUIRE(src.render_block(buf.data(), buf.size()) == 2);
    REQUIRE_THAT(buf[1].real(), WithinAbs(0.2 * std::cos(2.0 * M_PI * 100e3 / 1e6), 1e-6));
}

TEST_CASE("Source lifecycle: render after duration exhausted returns 0", "[dsp][lifecycle]") {
    CwSource src;
    src.configure({{"amplitude", 0.2}, {"frequency_rate", 1e6}, {"sample_rate", 1e6}, {"duration_sec", 0.001}});
    src.prepare();

    std::vector<std::complex<float>> buf(1000);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 1000);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);

    size_t n3 = src.render_block(buf.data(), buf.size());
    REQUIRE(n3 == 0);
}

TEST_CASE("Source lifecycle: metadata available after configure", "[dsp][lifecycle]") {
    ChirpSource src;
    src.configure({{"amplitude", 0.5}, {"f0_hz", -100e3}, {"f1_hz", 100e3}, {"sample_rate", 1e6}});
    src.prepare();

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(meta.sample_rate, WithinAbs(1e6, 1e-9));
}

TEST_CASE("Chirp source keeps prior configuration after invalid reconfigure", "[dsp][lifecycle]") {
    ChirpSource src;
    src.configure({{"amplitude", 0.5}, {"f0_hz", -100e3}, {"f1_hz", 100e3}, {"sample_rate", 1e6}});

    CHECK_THROWS_AS(src.configure({
                        {"amplitude", 0.8},
                        {"f0_hz", 0.0},
                        {"f1_hz", std::numeric_limits<double>::infinity()},
                        {"sample_rate", 2e6},
                    }),
                    std::invalid_argument);

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(meta.sample_rate, WithinAbs(1e6, 1e-9));
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(200e3, 1e-9));
}

TEST_CASE("Source lifecycle: noise source repeats indefinitely", "[dsp][lifecycle]") {
    NoiseSource src;
    src.configure({{"amplitude", 0.1}, {"sample_rate", 1e6}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(1000);

    for (int block = 0; block < 20; ++block) {
        size_t n = src.render_block(buf.data(), buf.size());
        REQUIRE(n == 1000);
    }
}
