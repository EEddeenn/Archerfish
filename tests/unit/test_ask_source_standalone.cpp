#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/ask_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("ASK source produces non-zero output", "[dsp][ask_source]") {
    AskSource src;
    src.configure({{"amplitude", 0.5},
                   {"frequency_hz", 0.0},
                   {"symbol_rate", 10e3},
                   {"sample_rate", 1e6},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(5000);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 5000);

    bool has_nonzero = false;
    for (const auto& s : buf) {
        if (std::abs(s) > 0.01f) has_nonzero = true;
    }
    REQUIRE(has_nonzero);
}

TEST_CASE("ASK source amplitude never exceeds configured amplitude", "[dsp][ask_source]") {
    AskSource src;
    double amp = 0.3;
    src.configure({{"amplitude", amp},
                   {"frequency_hz", 0.0},
                   {"symbol_rate", 10e3},
                   {"sample_rate", 1e6},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(5000);
    src.render_block(buf.data(), buf.size());

    for (const auto& s : buf) {
        REQUIRE(std::abs(s) <= static_cast<float>(amp * std::sqrt(2.0)) + 0.01f);
    }
}

TEST_CASE("ASK source duration limit respected", "[dsp][ask_source]") {
    AskSource src;
    src.configure({{"amplitude", 0.5},
                   {"frequency_hz", 0.0},
                   {"symbol_rate", 1e3},
                   {"sample_rate", 1000},
                   {"duration_sec", 0.01},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("ASK source reset restarts generation", "[dsp][ask_source]") {
    AskSource src;
    src.configure({{"amplitude", 0.5},
                   {"frequency_hz", 0.0},
                   {"symbol_rate", 1e3},
                   {"sample_rate", 1e6},
                   {"duration_sec", 0.001},
                   {"seed", 42}});
    src.prepare();

    std::vector<std::complex<float>> buf1(1000);
    src.render_block(buf1.data(), buf1.size());

    src.reset();

    std::vector<std::complex<float>> buf2(1000);
    size_t n = src.render_block(buf2.data(), buf2.size());
    REQUIRE(n == 1000);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf2[i].real(), WithinAbs(buf1[i].real(), 1e-6f));
        REQUIRE_THAT(buf2[i].imag(), WithinAbs(buf1[i].imag(), 1e-6f));
    }
}

TEST_CASE("ASK source metadata reports sample rate", "[dsp][ask_source]") {
    AskSource src;
    src.configure({{"amplitude", 0.5},
                   {"frequency_hz", 0.0},
                   {"symbol_rate", 1e3},
                   {"sample_rate", 2e6},
                   {"seed", 42}});
    src.prepare();

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.sample_rate, WithinAbs(2e6, 1e-9));
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.5 * std::sqrt(2.0), 1e-9));
}
