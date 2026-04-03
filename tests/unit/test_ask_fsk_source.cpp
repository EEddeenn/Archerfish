#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <set>
#include <vector>

#include "archerfish/dsp/ask_source.hpp"
#include "archerfish/dsp/fsk_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("ASK M=2 (OOK) produces on/off symbols", "[dsp][ask]") {
    AskSource src;
    double fs = 1e6;
    double sym_rate = 100e3;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"symbol_rate", sym_rate},
                   {"num_levels", 2},
                   {"seed", 42}});
    src.prepare();

    size_t sps = static_cast<size_t>(std::round(fs / sym_rate));
    size_t total = sps * 50;
    std::vector<std::complex<float>> buf(total);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == total);

    bool has_zero = false;
    bool has_nonzero = false;
    for (size_t sym = 0; sym < 50; ++sym) {
        float mag = std::abs(buf[sym * sps]);
        if (mag < 1e-6f) {
            has_zero = true;
        } else {
            has_nonzero = true;
            float expected = 0.2f * static_cast<float>(std::sqrt(2.0));
            REQUIRE_THAT(mag, WithinAbs(expected, 0.01f));
        }
    }
    REQUIRE(has_zero);
    REQUIRE(has_nonzero);
}

TEST_CASE("ASK M=4 produces distinct amplitude levels", "[dsp][ask]") {
    AskSource src;
    double fs = 1e6;
    double sym_rate = 100e3;
    src.configure({{"amplitude", 1.0},
                   {"frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"symbol_rate", sym_rate},
                   {"num_levels", 4},
                   {"seed", 42}});
    src.prepare();

    size_t sps = static_cast<size_t>(std::round(fs / sym_rate));
    size_t total = sps * 200;
    std::vector<std::complex<float>> buf(total);
    src.render_block(buf.data(), buf.size());

    std::set<float> levels;
    for (size_t sym = 0; sym < 200; ++sym) {
        float val = buf[sym * sps].real();
        float rounded = std::round(val * 1000.0f) / 1000.0f;
        levels.insert(rounded);
    }

    REQUIRE(levels.size() >= 3);
}

TEST_CASE("ASK seeded reproducibility", "[dsp][ask]") {
    AskSource src1, src2;
    auto params = nlohmann::json{{"amplitude", 0.2},
                                 {"frequency_hz", 0.0},
                                 {"sample_rate", 1e6},
                                 {"symbol_rate", 100e3},
                                 {"num_levels", 2},
                                 {"seed", 123}};
    src1.configure(params);
    src1.prepare();
    src2.configure(params);
    src2.prepare();

    std::vector<std::complex<float>> buf1(1000);
    std::vector<std::complex<float>> buf2(1000);
    src1.render_block(buf1.data(), buf1.size());
    src2.render_block(buf2.data(), buf2.size());

    for (size_t i = 0; i < 1000; ++i) {
        REQUIRE_THAT(buf1[i].real(), WithinAbs(buf2[i].real(), 1e-6f));
        REQUIRE_THAT(buf1[i].imag(), WithinAbs(buf2[i].imag(), 1e-6f));
    }
}

TEST_CASE("ASK block boundary across symbol", "[dsp][ask]") {
    AskSource src;
    double fs = 1e6;
    double sym_rate = 100e3;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"symbol_rate", sym_rate},
                   {"num_levels", 2},
                   {"seed", 42}});
    src.prepare();

    size_t sps = static_cast<size_t>(std::round(fs / sym_rate));

    std::vector<std::complex<float>> all(sps * 4);
    src.render_block(all.data(), all.size());

    src.reset();

    std::vector<std::complex<float>> split(sps * 4);
    size_t off = 0;
    while (off < split.size()) {
        size_t chunk = std::min(size_t(3), split.size() - off);
        off += src.render_block(split.data() + off, chunk);
    }

    for (size_t i = 0; i < all.size(); ++i) {
        REQUIRE_THAT(split[i].real(), WithinAbs(all[i].real(), 1e-6f));
        REQUIRE_THAT(split[i].imag(), WithinAbs(all[i].imag(), 1e-6f));
    }
}

TEST_CASE("FSK M=2 produces frequency changes between symbols", "[dsp][fsk]") {
    FskSource src;
    double fs = 1e6;
    double sym_rate = 10e3;
    double dev = 50e3;
    src.configure({{"amplitude", 1.0},
                   {"center_frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"symbol_rate", sym_rate},
                   {"modulation_order", 2},
                   {"deviation_hz", dev},
                   {"seed", 42}});
    src.prepare();

    size_t sps = static_cast<size_t>(std::round(fs / sym_rate));
    size_t total = sps * 10;
    std::vector<std::complex<float>> buf(total);
    src.render_block(buf.data(), buf.size());

    bool freq_change = false;
    for (size_t sym = 0; sym < 9; ++sym) {
        float phase1 = std::arg(buf[(sym + 1) * sps]);
        float phase0 = std::arg(buf[sym * sps + sps - 1]);
        float dp = phase1 - phase0;
        while (dp > M_PI) dp -= 2.0f * static_cast<float>(M_PI);
        while (dp < static_cast<float>(-M_PI)) dp += 2.0f * static_cast<float>(M_PI);
        float inst_freq = dp * static_cast<float>(fs) / static_cast<float>(sps);

        if (sym > 0) {
            float phase_prev = std::arg(buf[sym * sps]);
            float phase_prev_end = std::arg(buf[sym * sps - 1]);
            float dp_prev = phase_prev - phase_prev_end;
            while (dp_prev > static_cast<float>(M_PI)) dp_prev -= 2.0f * static_cast<float>(M_PI);
            while (dp_prev < static_cast<float>(-M_PI)) dp_prev += 2.0f * static_cast<float>(M_PI);
            float freq_prev = dp_prev * static_cast<float>(fs);

            if (std::abs(inst_freq - freq_prev) > 10e3f) {
                freq_change = true;
            }
        }
    }
    REQUIRE(freq_change);
}

TEST_CASE("FSK constant amplitude", "[dsp][fsk]") {
    FskSource src;
    src.configure({{"amplitude", 0.5},
                   {"center_frequency_hz", 0.0},
                   {"sample_rate", 1e6},
                   {"symbol_rate", 10e3},
                   {"modulation_order", 2},
                   {"deviation_hz", 50e3},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(1000);
    src.render_block(buf.data(), buf.size());

    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE_THAT(std::abs(buf[i]), WithinAbs(0.5f, 0.01f));
    }
}

TEST_CASE("FSK seeded reproducibility", "[dsp][fsk]") {
    FskSource src1, src2;
    auto params = nlohmann::json{{"amplitude", 0.2},
                                 {"center_frequency_hz", 0.0},
                                 {"sample_rate", 1e6},
                                 {"symbol_rate", 10e3},
                                 {"modulation_order", 2},
                                 {"deviation_hz", 50e3},
                                 {"seed", 77}};
    src1.configure(params);
    src1.prepare();
    src2.configure(params);
    src2.prepare();

    std::vector<std::complex<float>> buf1(500);
    std::vector<std::complex<float>> buf2(500);
    src1.render_block(buf1.data(), buf1.size());
    src2.render_block(buf2.data(), buf2.size());

    for (size_t i = 0; i < 500; ++i) {
        REQUIRE_THAT(buf1[i].real(), WithinAbs(buf2[i].real(), 1e-6f));
        REQUIRE_THAT(buf1[i].imag(), WithinAbs(buf2[i].imag(), 1e-6f));
    }
}

TEST_CASE("FSK block boundary across symbol", "[dsp][fsk]") {
    FskSource src;
    double fs = 1e6;
    double sym_rate = 100e3;
    src.configure({{"amplitude", 0.2},
                   {"center_frequency_hz", 0.0},
                   {"sample_rate", fs},
                   {"symbol_rate", sym_rate},
                   {"modulation_order", 2},
                   {"deviation_hz", 10e3},
                   {"seed", 42}});
    src.prepare();

    size_t sps = static_cast<size_t>(std::round(fs / sym_rate));

    std::vector<std::complex<float>> all(sps * 4);
    src.render_block(all.data(), all.size());

    src.reset();

    std::vector<std::complex<float>> split(sps * 4);
    size_t off = 0;
    while (off < split.size()) {
        size_t chunk = std::min(size_t(3), split.size() - off);
        off += src.render_block(split.data() + off, chunk);
    }

    for (size_t i = 0; i < all.size(); ++i) {
        REQUIRE_THAT(split[i].real(), WithinAbs(all[i].real(), 1e-6f));
        REQUIRE_THAT(split[i].imag(), WithinAbs(all[i].imag(), 1e-6f));
    }
}
