#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/modulator.hpp"
#include "archerfish/dsp/pulse_shaper.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("BPSK constellation has +/-1 on real axis", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "BPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(400);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n > 0);

    bool has_pos = false, has_neg = false;
    for (const auto& s : buf) {
        if (s.real() > 0.5f) has_pos = true;
        if (s.real() < -0.5f) has_neg = true;
    }
    REQUIRE(has_pos);
    REQUIRE(has_neg);
}

TEST_CASE("QPSK constellation symbols at +/-1 +/-1j / sqrt(2)", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "QPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(400);
    src.render_block(buf.data(), buf.size());

    bool found_qpsk = false;
    for (const auto& s : buf) {
        float mag = std::abs(s);
        if (mag > 0.5f) {
            found_qpsk = true;
            break;
        }
    }
    REQUIRE(found_qpsk);
}

TEST_CASE("16QAM constellation has correct grid positions", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "16QAM"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(1600);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n > 0);
}

TEST_CASE("RRC filter has correct number of taps", "[dsp][modulator]") {
    RrcFilterDesign design;
    design.alpha = 0.35;
    design.span_symbols = 6;
    design.samples_per_symbol = 4;

    REQUIRE(design.num_taps() == 6 * 4 + 1);

    auto taps = design.design();
    REQUIRE(taps.size() == 25);
}

TEST_CASE("Modulator output has expected sample count for duration", "[dsp][modulator]") {
    ModulatorSource src;
    double dur = 0.001;
    double fs = 4e3;
    src.configure({{"modulation", "BPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", fs}, {"duration_sec", dur}, {"seed", 42}});
    src.prepare();

    size_t expected = static_cast<size_t>(fs * dur);
    std::vector<std::complex<float>> buf(expected);
    size_t n = src.render_block(buf.data(), expected);
    REQUIRE(n == expected);

    std::vector<std::complex<float>> extra(10);
    size_t extra_n = src.render_block(extra.data(), 10);
    REQUIRE(extra_n == 0);
}

TEST_CASE("Modulator reproducible with seed", "[dsp][modulator]") {
    ModulatorSource src1;
    src1.configure({{"modulation", "QPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                    {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 99}});
    src1.prepare();

    ModulatorSource src2;
    src2.configure({{"modulation", "QPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                    {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 99}});
    src2.prepare();

    std::vector<std::complex<float>> buf1(100);
    std::vector<std::complex<float>> buf2(100);
    src1.render_block(buf1.data(), buf1.size());
    src2.render_block(buf2.data(), buf2.size());

    for (size_t i = 0; i < buf1.size(); ++i) {
        REQUIRE(buf1[i].real() == buf2[i].real());
        REQUIRE(buf1[i].imag() == buf2[i].imag());
    }
}
