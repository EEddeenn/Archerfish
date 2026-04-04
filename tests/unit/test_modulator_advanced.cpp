#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/modulator.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("8PSK modulator produces output", "[dsp][modulator][advanced]") {
    ModulatorSource src;
    src.configure({{"modulation", "8PSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(400);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n > 0);
}

TEST_CASE("64QAM modulator produces output", "[dsp][modulator][advanced]") {
    ModulatorSource src;
    src.configure({{"modulation", "64QAM"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(1600);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n > 0);

    auto meta = src.report_metadata();
    REQUIRE(meta.crest_factor > 1.0);
}

TEST_CASE("APSK32 modulator produces output", "[dsp][modulator][advanced]") {
    ModulatorSource src;
    src.configure({{"modulation", "APSK32"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(1600);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n > 0);

    const auto& c = src.constellation();
    REQUIRE(c.size() == 32);
}

TEST_CASE("BPSK modulator with different RRC alpha", "[dsp][modulator][advanced]") {
    for (double alpha : {0.1, 0.5, 1.0}) {
        ModulatorSource src;
        src.configure({{"modulation", "BPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 8},
                       {"rrc_alpha", alpha}, {"amplitude", 1.0}, {"sample_rate", 8e3}, {"seed", 42}});
        src.prepare();

        std::vector<std::complex<float>> buf(800);
        size_t n = src.render_block(buf.data(), buf.size());
        REQUIRE(n > 0);

        auto meta = src.report_metadata();
        REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(1e3 * (1.0 + alpha), 1e-6));
    }
}

TEST_CASE("Modulator reset produces identical output", "[dsp][modulator][advanced]") {
    ModulatorSource src;
    src.configure({{"modulation", "QPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 99}});
    src.prepare();

    std::vector<std::complex<float>> buf1(200);
    src.render_block(buf1.data(), buf1.size());

    src.reset();

    std::vector<std::complex<float>> buf2(200);
    src.render_block(buf2.data(), buf2.size());

    for (size_t i = 0; i < buf1.size(); ++i) {
        REQUIRE(buf1[i].real() == buf2[i].real());
        REQUIRE(buf1[i].imag() == buf2[i].imag());
    }
}
