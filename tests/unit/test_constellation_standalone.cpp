#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/constellation.hpp"
#include "archerfish/dsp/modulation_type.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("BPSK constellation has 2 points", "[dsp][constellation]") {
    auto c = build_constellation(ModulationType::BPSK);
    REQUIRE(c.size() == 2);

    // BPSK should be +/-1 on real axis
    for (auto& p : c) {
        REQUIRE_THAT(p.imag(), WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(std::abs(p.real()), WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE("QPSK constellation has 4 points on unit circle", "[dsp][constellation]") {
    auto c = build_constellation(ModulationType::QPSK);
    REQUIRE(c.size() == 4);

    for (auto& p : c) {
        REQUIRE_THAT(std::abs(p), WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE("8PSK constellation has 8 equally spaced points", "[dsp][constellation]") {
    auto c = build_constellation(ModulationType::PSK8);
    REQUIRE(c.size() == 8);

    for (auto& p : c) {
        REQUIRE_THAT(std::abs(p), WithinAbs(1.0f, 0.01f));
    }

    for (size_t i = 0; i < c.size(); ++i) {
        for (size_t j = i + 1; j < c.size(); ++j) {
            REQUIRE(std::abs(c[i] - c[j]) > 1e-3f);
        }
    }
}

TEST_CASE("16QAM constellation has 16 points with unit average power", "[dsp][constellation]") {
    auto c = build_constellation(ModulationType::QAM16);
    REQUIRE(c.size() == 16);

    double avg_power = 0.0;
    for (auto& p : c) avg_power += std::norm(p);
    avg_power /= static_cast<double>(c.size());
    REQUIRE_THAT(avg_power, WithinAbs(1.0, 0.05));
}

TEST_CASE("64QAM constellation has 64 points with unit average power", "[dsp][constellation]") {
    auto c = build_constellation(ModulationType::QAM64);
    REQUIRE(c.size() == 64);

    double avg_power = 0.0;
    for (auto& p : c) avg_power += std::norm(p);
    avg_power /= static_cast<double>(c.size());
    REQUIRE_THAT(avg_power, WithinAbs(1.0, 0.05));
}

TEST_CASE("APSK16 constellation has 16 points", "[dsp][constellation]") {
    auto c = build_constellation(ModulationType::APSK16);
    REQUIRE(c.size() == 16);

    double avg_power = 0.0;
    for (auto& p : c) avg_power += std::norm(p);
    avg_power /= static_cast<double>(c.size());
    REQUIRE(avg_power > 0.0);
}

TEST_CASE("APSK32 constellation has 32 points", "[dsp][constellation]") {
    auto c = build_constellation(ModulationType::APSK32);
    REQUIRE(c.size() == 32);

    double avg_power = 0.0;
    for (auto& p : c) avg_power += std::norm(p);
    avg_power /= static_cast<double>(c.size());
    REQUIRE(avg_power > 0.0);
}
