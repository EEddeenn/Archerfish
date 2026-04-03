#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include "archerfish/dsp/modulator.hpp"
#include "archerfish/impairments/impairment_chain.hpp"
#include "archerfish/impairments/awgn.hpp"
#include "archerfish/impairments/cfo.hpp"
#include "archerfish/impairments/phase_offset.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("QPSK with AWGN produces noisier output than clean", "[integration][qpsk][impairments]") {
    const size_t sps = 8;
    const double symbol_rate = 1e3;
    const double sample_rate = sps * symbol_rate;
    const size_t total_samples = 1024;

    archerfish::dsp::ModulatorSource src;
    src.configure({{"modulation", "QPSK"},
                   {"symbol_rate", symbol_rate},
                   {"samples_per_symbol", sps},
                   {"amplitude", 1.0},
                   {"sample_rate", sample_rate},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> clean(total_samples);
    size_t n = src.render_block(clean.data(), clean.size());
    REQUIRE(n == total_samples);

    double clean_power = 0.0;
    for (auto& s : clean) clean_power += std::abs(s) * std::abs(s);
    clean_power /= static_cast<double>(n);

    archerfish::dsp::ModulatorSource src2;
    src2.configure({{"modulation", "QPSK"},
                    {"symbol_rate", symbol_rate},
                    {"samples_per_symbol", sps},
                    {"amplitude", 1.0},
                    {"sample_rate", sample_rate},
                    {"seed", 42}});
    src2.prepare();

    std::vector<std::complex<float>> noisy(total_samples);
    size_t n2 = src2.render_block(noisy.data(), noisy.size());
    REQUIRE(n2 == total_samples);

    archerfish::impairments::AwgnImpairment awgn(0.01, 999);
    awgn.apply(noisy.data(), noisy.size());

    double noisy_power = 0.0;
    for (auto& s : noisy) noisy_power += std::abs(s) * std::abs(s);
    noisy_power /= static_cast<double>(n2);

    REQUIRE(noisy_power > clean_power);
}

TEST_CASE("QPSK with CFO rotates constellation over time", "[integration][qpsk][impairments]") {
    const size_t sps = 8;
    const double symbol_rate = 1e3;
    const double sample_rate = sps * symbol_rate;
    const size_t total_samples = 1024;

    archerfish::dsp::ModulatorSource src;
    src.configure({{"modulation", "QPSK"},
                   {"symbol_rate", symbol_rate},
                   {"samples_per_symbol", sps},
                   {"amplitude", 1.0},
                   {"sample_rate", sample_rate},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(total_samples);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == total_samples);

    archerfish::impairments::CfoImpairment cfo(100.0, sample_rate);
    cfo.apply(buf.data(), buf.size());

    float phase_start = std::arg(buf[0]);
    float phase_end = std::arg(buf[total_samples - 1]);
    float phase_diff = std::abs(phase_end - phase_start);
    REQUIRE(phase_diff > 0.01f);
}

TEST_CASE("QPSK with phase offset rotates all symbols", "[integration][qpsk][impairments]") {
    const size_t sps = 8;
    const double symbol_rate = 1e3;
    const double sample_rate = sps * symbol_rate;
    const size_t total_samples = 512;

    archerfish::dsp::ModulatorSource src_clean;
    src_clean.configure({{"modulation", "QPSK"},
                         {"symbol_rate", symbol_rate},
                         {"samples_per_symbol", sps},
                         {"amplitude", 1.0},
                         {"sample_rate", sample_rate},
                         {"seed", 42}});
    src_clean.prepare();

    std::vector<std::complex<float>> clean(total_samples);
    src_clean.render_block(clean.data(), clean.size());

    archerfish::dsp::ModulatorSource src_offset;
    src_offset.configure({{"modulation", "QPSK"},
                          {"symbol_rate", symbol_rate},
                          {"samples_per_symbol", sps},
                          {"amplitude", 1.0},
                          {"sample_rate", sample_rate},
                          {"seed", 42}});
    src_offset.prepare();

    std::vector<std::complex<float>> rotated(total_samples);
    src_offset.render_block(rotated.data(), rotated.size());

    double phase_offset = 0.5;
    archerfish::impairments::PhaseOffsetImpairment po(phase_offset);
    po.apply(rotated.data(), rotated.size());

    for (size_t i = 0; i < total_samples; ++i) {
        float expected_phase = std::arg(clean[i]) + static_cast<float>(phase_offset);
        float actual_phase = std::arg(rotated[i]);
        float diff = std::abs(actual_phase - expected_phase);
        if (diff > static_cast<float>(M_PI)) diff = 2.0f * static_cast<float>(M_PI) - diff;
        REQUIRE(diff < 0.01f);
    }
}

TEST_CASE("QPSK with impairment chain applies all impairments", "[integration][qpsk][impairments]") {
    const size_t sps = 8;
    const double symbol_rate = 1e3;
    const double sample_rate = sps * symbol_rate;
    const size_t total_samples = 1024;

    archerfish::dsp::ModulatorSource src;
    src.configure({{"modulation", "QPSK"},
                   {"symbol_rate", symbol_rate},
                   {"samples_per_symbol", sps},
                   {"amplitude", 1.0},
                   {"sample_rate", sample_rate},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(total_samples);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == total_samples);

    std::vector<std::complex<float>> original(buf.begin(), buf.end());

    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::CfoImpairment>(50.0, sample_rate));
    chain.add(std::make_unique<archerfish::impairments::PhaseOffsetImpairment>(0.3));
    chain.add(std::make_unique<archerfish::impairments::AwgnImpairment>(0.005, 12345));

    REQUIRE(chain.size() == 3);
    chain.apply(buf.data(), buf.size());

    double diff_sum = 0.0;
    for (size_t i = 0; i < total_samples; ++i) {
        diff_sum += std::abs(buf[i] - original[i]);
    }
    double avg_diff = diff_sum / static_cast<double>(total_samples);
    REQUIRE(avg_diff > 0.0);
}
