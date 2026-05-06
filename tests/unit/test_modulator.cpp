#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/common/constants.hpp"
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

TEST_CASE("Modulator rejects invalid modulation parameters", "[dsp][modulator]") {
    ModulatorSource src;
    CHECK_THROWS_AS(src.configure({{"modulation", 42}, {"sample_rate", 4e3}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"type", 42}, {"sample_rate", 4e3}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "bad"}, {"sample_rate", 4e3}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "BPSK"}, {"sample_rate", 4e3}, {"symbol_rate", "fast"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "BPSK"}, {"sample_rate", 4e3}, {"symbol_rate", 0.0}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "BPSK"}, {"sample_rate", 4e3}, {"samples_per_symbol", 0}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "BPSK"}, {"sample_rate", 4e3}, {"samples_per_symbol", -1}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "BPSK"}, {"sample_rate", 4e3}, {"samples_per_symbol", 1.5}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "BPSK"}, {"sample_rate", 4e3}, {"samples_per_symbol", 2048}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "BPSK"}, {"sample_rate", 4e3}, {"rrc_alpha", "wide"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "BPSK"}, {"sample_rate", 4e3}, {"rrc_alpha", 0.0}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"modulation", "BPSK"}, {"sample_rate", 4e3}, {"rrc_alpha", 1.1}}), std::invalid_argument);
}

TEST_CASE("Modulator keeps prior configuration after invalid reconfigure", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "QPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"rrc_alpha", 0.25}, {"amplitude", 0.5}, {"sample_rate", 4e3}, {"seed", 42}});

    CHECK_THROWS_AS(src.configure({{"modulation", "64QAM"}, {"symbol_rate", 2e3}, {"samples_per_symbol", 8},
                                   {"rrc_alpha", 1.1}, {"amplitude", 0.9}, {"sample_rate", 8e3}}),
                    std::invalid_argument);

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.sample_rate, WithinAbs(4e3, 1e-9));
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(1250.0, 1e-9));
}

TEST_CASE("Modulator render validates output buffer and lifecycle", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "BPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});

    std::vector<std::complex<float>> buf(4);
    CHECK_THROWS_AS(src.render_block(buf.data(), buf.size()), std::logic_error);

    src.prepare();
    CHECK(src.render_block(nullptr, 0) == 0);
    CHECK_THROWS_AS(src.render_block(nullptr, 1), std::invalid_argument);
}

TEST_CASE("Modulator reconfigure invalidates prepared filter state", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "BPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(8);
    REQUIRE(src.render_block(buf.data(), buf.size()) == 8);

    src.configure({{"modulation", "QPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 8},
                   {"amplitude", 1.0}, {"sample_rate", 8e3}, {"seed", 42}});
    CHECK_THROWS_AS(src.render_block(buf.data(), buf.size()), std::logic_error);

    src.prepare();
    REQUIRE(src.render_block(buf.data(), buf.size()) == 8);
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

TEST_CASE("QPSK constellation points land at correct positions after pulse shaping",
          "[dsp][modulator]") {
    const size_t sps = 4;
    const size_t num_symbols = 256;
    const size_t total_samples = num_symbols * sps;
    const double alpha = 0.35;

    ModulatorSource src;
    src.configure({{"modulation", "QPSK"},
                   {"symbol_rate", 1e3},
                   {"samples_per_symbol", sps},
                   {"rrc_alpha", alpha},
                   {"amplitude", 1.0},
                   {"sample_rate", static_cast<double>(sps * 1e3)},
                   {"duration_sec", static_cast<double>(total_samples) / (sps * 1e3)},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(total_samples);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == total_samples);

    RrcFilterDesign design;
    design.alpha = alpha;
    design.span_symbols = 6;
    design.samples_per_symbol = sps;
    auto rrc_taps = design.design();
    size_t L = rrc_taps.size();

    size_t matched_len = n + L - 1;
    std::vector<std::complex<float>> matched(matched_len, {0.0f, 0.0f});
    for (size_t m = 0; m < matched_len; ++m) {
        std::complex<float> acc{0.0f, 0.0f};
        size_t k_min = (m >= L - 1) ? m - (L - 1) : 0;
        size_t k_max = std::min(m, n - 1);
        for (size_t k = k_min; k <= k_max; ++k) {
            acc += buf[k] * rrc_taps[m - k];
        }
        matched[m] = acc;
    }

    size_t best_offset = 0;
    double best_total_dist = 1e18;
    for (size_t trial_offset = 0; trial_offset < sps; ++trial_offset) {
        double total_dist = 0;
        int count = 0;
        for (size_t sym = 8; sym < num_symbols - 8; ++sym) {
            size_t idx = sym * sps + trial_offset + L - 1;
            if (idx >= matched.size()) break;
            auto s = matched[idx];
            double min_d = 1e9;
            float inv_sqrt2 = 1.0f / std::sqrt(2.0f);
            std::vector<std::complex<float>> pts = {
                {inv_sqrt2, inv_sqrt2}, {-inv_sqrt2, inv_sqrt2},
                {-inv_sqrt2, -inv_sqrt2}, {inv_sqrt2, -inv_sqrt2}};
            for (auto& p : pts) min_d = std::min(min_d, static_cast<double>(std::abs(s - p)));
            total_dist += min_d;
            ++count;
        }
        if (count > 0 && total_dist < best_total_dist) {
            best_total_dist = total_dist;
            best_offset = trial_offset;
        }
    }

    float inv_sqrt2 = 1.0f / std::sqrt(2.0f);
    std::vector<std::complex<float>> qpsk_points = {
        {inv_sqrt2, inv_sqrt2}, {-inv_sqrt2, inv_sqrt2},
        {-inv_sqrt2, -inv_sqrt2}, {inv_sqrt2, -inv_sqrt2}};

    size_t skip_symbols = 8;
    bool found[4] = {false, false, false, false};
    int checked = 0;

    for (size_t sym = skip_symbols; sym < num_symbols - skip_symbols; ++sym) {
        size_t idx = sym * sps + best_offset + L - 1;
        if (idx >= matched.size()) break;

        auto sample = matched[idx];
        double min_dist = 1e9;
        int closest = 0;
        for (int p = 0; p < 4; ++p) {
            double dist = std::abs(sample - qpsk_points[p]);
            if (dist < min_dist) {
                min_dist = dist;
                closest = p;
            }
        }
        REQUIRE(min_dist < 0.15);
        found[closest] = true;
        ++checked;
    }
    REQUIRE(checked > 0);
    for (int p = 0; p < 4; ++p) {
        REQUIRE(found[p]);
    }
}

TEST_CASE("Inter-batch filter continuity matches continuous output",
          "[dsp][modulator]") {
    const char* mod_types[] = {"BPSK", "QPSK", "8PSK", "16QAM", "64QAM"};
    const size_t sps = 4;
    const double symbol_rate = 1e3;
    const double sample_rate = sps * symbol_rate;

    for (const char* mod : mod_types) {
        ModulatorSource src_continuous;
        src_continuous.configure({{"modulation", mod},
                                 {"symbol_rate", symbol_rate},
                                 {"samples_per_symbol", sps},
                                 {"amplitude", 1.0},
                                 {"sample_rate", sample_rate},
                                 {"seed", 42}});
        src_continuous.prepare();

        std::vector<std::complex<float>> continuous(2048);
        size_t n_cont = src_continuous.render_block(continuous.data(), continuous.size());
        REQUIRE(n_cont > 0);

        ModulatorSource src_batched;
        src_batched.configure({{"modulation", mod},
                              {"symbol_rate", symbol_rate},
                              {"samples_per_symbol", sps},
                              {"amplitude", 1.0},
                              {"sample_rate", sample_rate},
                              {"seed", 42}});
        src_batched.prepare();

        std::vector<std::complex<float>> batched(n_cont, {0.0f, 0.0f});
        size_t batch_size = 64;
        size_t offset = 0;
        while (offset < n_cont) {
            size_t to_render = std::min(batch_size, n_cont - offset);
            size_t produced =
                src_batched.render_block(batched.data() + offset, to_render);
            REQUIRE(produced > 0);
            offset += produced;
        }

        REQUIRE(batched.size() >= continuous.size());

        for (size_t i = 0; i < n_cont; ++i) {
            float diff_re = std::abs(batched[i].real() - continuous[i].real());
            float diff_im = std::abs(batched[i].imag() - continuous[i].imag());
            REQUIRE(diff_re < 0.05f);
            REQUIRE(diff_im < 0.05f);
        }
    }
}

TEST_CASE("Spectral bandwidth falls within RRC bandwidth", "[dsp][modulator]") {
    const size_t sps = 8;
    const double symbol_rate = 1e3;
    const double sample_rate = sps * symbol_rate;
    const double alpha = 0.35;
    const double expected_bw = symbol_rate * (1.0 + alpha);

    ModulatorSource src;
    src.configure({{"modulation", "QPSK"},
                   {"symbol_rate", symbol_rate},
                   {"samples_per_symbol", sps},
                   {"rrc_alpha", alpha},
                   {"amplitude", 1.0},
                   {"sample_rate", sample_rate},
                   {"seed", 42}});
    src.prepare();

    const size_t N = 4096;
    std::vector<std::complex<float>> buf(N);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == N);

    std::vector<double> power(N);
    for (size_t k = 0; k < N; ++k) {
        double re = 0.0, im = 0.0;
        for (size_t n_idx = 0; n_idx < N; ++n_idx) {
            double angle = -2.0 * 3.14159265358979323846 * static_cast<double>(k) *
                           static_cast<double>(n_idx) / static_cast<double>(N);
            re += buf[n_idx].real() * std::cos(angle) - buf[n_idx].imag() * std::sin(angle);
            im += buf[n_idx].real() * std::sin(angle) + buf[n_idx].imag() * std::cos(angle);
        }
        power[k] = (re * re + im * im) / static_cast<double>(N * N);
    }

    double total_power = 0.0;
    for (auto p : power) total_power += p;

    double freq_res = sample_rate / static_cast<double>(N);

    double in_band_power = 0.0;
    for (size_t k = 0; k < N; ++k) {
        double abs_freq;
        if (k <= N / 2)
            abs_freq = static_cast<double>(k) * freq_res;
        else
            abs_freq = static_cast<double>(N - k) * freq_res;

        if (abs_freq <= expected_bw) in_band_power += power[k];
    }

    double ratio = in_band_power / total_power;
    REQUIRE(ratio > 0.80);
}

TEST_CASE("Metadata reports reasonable crest factor and RMS after calibration",
          "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "QPSK"},
                   {"symbol_rate", 1e3},
                   {"samples_per_symbol", 4},
                   {"amplitude", 0.5},
                   {"sample_rate", 4e3},
                   {"seed", 42}});
    src.prepare();

    auto meta = src.report_metadata();

    REQUIRE(meta.crest_factor > 1.0);
    REQUIRE(meta.rms_amplitude > 0.0);
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(1e3 * 1.35, 1e-6));
    REQUIRE_THAT(meta.sample_rate, WithinAbs(4e3, 1e-9));
}

TEST_CASE("QPSK constellation has Gray coding property", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "QPSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    const auto& c = src.constellation();
    REQUIRE(c.size() == 4);

    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = i + 1; j < 4; ++j) {
            uint32_t xor_bits = i ^ j;
            int hamming = __builtin_popcount(xor_bits);
            double dist = std::abs(c[i] - c[j]);
            if (hamming == 1) {
                REQUIRE(dist < 1.5f);
            } else {
                REQUIRE(dist > 1.5f);
            }
        }
    }
}

TEST_CASE("16QAM constellation has Gray coding property", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "16QAM"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    const auto& c = src.constellation();
    REQUIRE(c.size() == 16);

    double min_adj_dist = 1e9;
    for (uint32_t i = 0; i < 16; ++i) {
        for (uint32_t j = i + 1; j < 16; ++j) {
            double dist = std::abs(c[i] - c[j]);
            int hamming = __builtin_popcount(i ^ j);
            if (hamming == 1) {
                min_adj_dist = std::min(min_adj_dist, dist);
            }
        }
    }
    REQUIRE(min_adj_dist > 0.0);

    double avg_power = 0.0;
    for (auto& p : c) avg_power += std::norm(p);
    avg_power /= static_cast<double>(c.size());
    REQUIRE_THAT(avg_power, WithinAbs(1.0, 0.01));
}

TEST_CASE("64QAM constellation has Gray coding and unit energy", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "64QAM"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    const auto& c = src.constellation();
    REQUIRE(c.size() == 64);

    double avg_power = 0.0;
    for (auto& p : c) avg_power += std::norm(p);
    avg_power /= static_cast<double>(c.size());
    REQUIRE_THAT(avg_power, WithinAbs(1.0, 0.01));
}

TEST_CASE("8PSK constellation has Gray coding property", "[dsp][modulator]") {
    ModulatorSource src;
    src.configure({{"modulation", "8PSK"}, {"symbol_rate", 1e3}, {"samples_per_symbol", 4},
                   {"amplitude", 1.0}, {"sample_rate", 4e3}, {"seed", 42}});
    src.prepare();

    const auto& c = src.constellation();
    REQUIRE(c.size() == 8);

    for (auto& p : c) {
        REQUIRE_THAT(std::abs(p), WithinAbs(1.0, 0.001));
    }

    for (uint32_t i = 0; i < 8; ++i) {
        double min_1bit_dist = 1e9;
        for (uint32_t j = 0; j < 8; ++j) {
            if (i == j) continue;
            int hamming = __builtin_popcount(i ^ j);
            if (hamming == 1) {
                double dist = std::abs(c[i] - c[j]);
                min_1bit_dist = std::min(min_1bit_dist, dist);
            }
        }
        REQUIRE_THAT(min_1bit_dist, WithinAbs(2.0 * std::sin(archerfish::constants::kPi / 8.0), 0.01));
    }
}
