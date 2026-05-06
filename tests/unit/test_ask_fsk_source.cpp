#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <set>
#include <vector>

#include "archerfish/common/constants.hpp"
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

TEST_CASE("ASK rejects invalid modulation parameters", "[dsp][ask]") {
    AskSource src;
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"frequency_hz", "high"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", "fast"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 0.0}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"frequency_hz", std::numeric_limits<double>::quiet_NaN()}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 100e3}, {"num_levels", "many"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 100e3}, {"num_levels", 2.5}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 100e3}, {"num_levels", 1}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 100e3}, {"num_levels", 1025}}), std::invalid_argument);
}

TEST_CASE("ASK keeps prior configuration after invalid reconfigure", "[dsp][ask]") {
    AskSource src;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", 1e6},
                   {"symbol_rate", 100e3},
                   {"num_levels", 2},
                   {"seed", 42}});

    CHECK_THROWS_AS(src.configure({{"amplitude", 0.8},
                                   {"frequency_hz", 10e3},
                                   {"sample_rate", 2e6},
                                   {"symbol_rate", 200e3},
                                   {"num_levels", 1}}),
                    std::invalid_argument);

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.sample_rate, WithinAbs(1e6, 1e-9));
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(100e3, 1e-9));
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.2 * std::sqrt(2.0), 1e-9));
}

TEST_CASE("ASK render validates output buffer and sample-rate relationship", "[dsp][ask]") {
    AskSource src;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", 1e6},
                   {"symbol_rate", 100e3},
                   {"num_levels", 2},
                   {"seed", 42}});
    src.prepare();

    CHECK(src.render_block(nullptr, 0) == 0);
    CHECK_THROWS_AS(src.render_block(nullptr, 1), std::invalid_argument);

    AskSource too_fast;
    too_fast.configure({{"sample_rate", 1e6}, {"symbol_rate", 3e6}});
    CHECK_THROWS_AS(too_fast.prepare(), std::invalid_argument);

    AskSource too_slow;
    too_slow.configure({{"sample_rate", std::numeric_limits<double>::max()}, {"symbol_rate", 0.5}});
    CHECK_THROWS_AS(too_slow.prepare(), std::overflow_error);
}

TEST_CASE("ASK reconfigure invalidates prepared symbol state", "[dsp][ask]") {
    AskSource src;
    src.configure({{"amplitude", 0.2},
                   {"frequency_hz", 0.0},
                   {"sample_rate", 1e6},
                   {"symbol_rate", 100e3},
                   {"num_levels", 2},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(10);
    REQUIRE(src.render_block(buf.data(), buf.size()) == 10);

    src.configure({{"sample_rate", 1e6}, {"symbol_rate", 50e3}, {"num_levels", 4}});
    CHECK_THROWS_AS(src.render_block(buf.data(), buf.size()), std::logic_error);

    src.prepare();
    REQUIRE(src.render_block(buf.data(), buf.size()) == 10);
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

TEST_CASE("ASK wraps high carrier phase modulo one cycle", "[dsp][ask]") {
    AskSource src;
    src.configure({{"amplitude", 1.0},
                   {"frequency_hz", 2.5e6},
                   {"sample_rate", 1e6},
                   {"symbol_rate", 1e3},
                   {"num_levels", 4},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(4);
    REQUIRE(src.render_block(buf.data(), buf.size()) == buf.size());

    REQUIRE_THAT(buf[2].real(), WithinAbs(buf[0].real(), 1e-5f));
    REQUIRE_THAT(buf[2].imag(), WithinAbs(buf[0].imag(), 1e-5f));
    REQUIRE_THAT(buf[1].real(), WithinAbs(-buf[0].real(), 1e-5f));
    REQUIRE_THAT(buf[1].imag(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(buf[3].real(), WithinAbs(buf[1].real(), 1e-5f));
    REQUIRE_THAT(buf[3].imag(), WithinAbs(0.0f, 1e-5f));
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
        while (dp > archerfish::constants::kPi) dp -= 2.0f * static_cast<float>(archerfish::constants::kPi);
        while (dp < static_cast<float>(-archerfish::constants::kPi)) dp += 2.0f * static_cast<float>(archerfish::constants::kPi);
        float inst_freq = dp * static_cast<float>(fs) / static_cast<float>(sps);

        if (sym > 0) {
            float phase_prev = std::arg(buf[sym * sps]);
            float phase_prev_end = std::arg(buf[sym * sps - 1]);
            float dp_prev = phase_prev - phase_prev_end;
            while (dp_prev > static_cast<float>(archerfish::constants::kPi)) dp_prev -= 2.0f * static_cast<float>(archerfish::constants::kPi);
            while (dp_prev < static_cast<float>(-archerfish::constants::kPi)) dp_prev += 2.0f * static_cast<float>(archerfish::constants::kPi);
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

TEST_CASE("FSK rejects invalid modulation parameters", "[dsp][fsk]") {
    FskSource src;
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"center_frequency_hz", "high"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", "fast"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"deviation_hz", "wide"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 0.0}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"center_frequency_hz", std::numeric_limits<double>::infinity()}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 10e3}, {"modulation_order", "many"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 10e3}, {"modulation_order", 2.5}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 10e3}, {"modulation_order", 1}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 10e3}, {"modulation_order", 1025}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e6}, {"symbol_rate", 10e3}, {"deviation_hz", -1.0}}), std::invalid_argument);
}

TEST_CASE("FSK keeps prior configuration after invalid reconfigure", "[dsp][fsk]") {
    FskSource src;
    src.configure({{"amplitude", 0.2},
                   {"center_frequency_hz", 0.0},
                   {"sample_rate", 1e6},
                   {"symbol_rate", 10e3},
                   {"modulation_order", 4},
                   {"deviation_hz", 50e3},
                   {"seed", 77}});

    CHECK_THROWS_AS(src.configure({{"amplitude", 0.8},
                                   {"center_frequency_hz", 10e3},
                                   {"sample_rate", 2e6},
                                   {"symbol_rate", 20e3},
                                   {"modulation_order", 8},
                                   {"deviation_hz", -1.0}}),
                    std::invalid_argument);

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.sample_rate, WithinAbs(1e6, 1e-9));
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.2, 1e-9));
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(110e3, 1e-9));
}

TEST_CASE("FSK render validates output buffer and sample-rate relationship", "[dsp][fsk]") {
    FskSource src;
    src.configure({{"amplitude", 0.2},
                   {"center_frequency_hz", 0.0},
                   {"sample_rate", 1e6},
                   {"symbol_rate", 100e3},
                   {"modulation_order", 2},
                   {"deviation_hz", 10e3},
                   {"seed", 42}});
    src.prepare();

    CHECK(src.render_block(nullptr, 0) == 0);
    CHECK_THROWS_AS(src.render_block(nullptr, 1), std::invalid_argument);

    FskSource too_fast;
    too_fast.configure({{"sample_rate", 1e6}, {"symbol_rate", 3e6}});
    CHECK_THROWS_AS(too_fast.prepare(), std::invalid_argument);

    FskSource too_slow;
    too_slow.configure({{"sample_rate", std::numeric_limits<double>::max()}, {"symbol_rate", 0.5}});
    CHECK_THROWS_AS(too_slow.prepare(), std::overflow_error);
}

TEST_CASE("FSK reconfigure invalidates prepared symbol state", "[dsp][fsk]") {
    FskSource src;
    src.configure({{"amplitude", 0.2},
                   {"center_frequency_hz", 0.0},
                   {"sample_rate", 1e6},
                   {"symbol_rate", 100e3},
                   {"modulation_order", 2},
                   {"deviation_hz", 10e3},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(10);
    REQUIRE(src.render_block(buf.data(), buf.size()) == 10);

    src.configure({{"sample_rate", 1e6}, {"symbol_rate", 50e3}, {"modulation_order", 4}});
    CHECK_THROWS_AS(src.render_block(buf.data(), buf.size()), std::logic_error);

    src.prepare();
    REQUIRE(src.render_block(buf.data(), buf.size()) == 10);
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
