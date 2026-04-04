#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/phase_noise.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("PhaseNoise very small magnitude approximately transparent", "[impairments][phase_noise][parameters]") {
    PhaseNoiseImpairment pn(100.0, 0.001, 1e6);
    std::vector<std::complex<float>> data(100, {1.0f, 0.0f});
    auto copy = data;

    pn.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(copy[i].real(), 0.01f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(copy[i].imag(), 0.01f));
    }
}

TEST_CASE("PhaseNoise large magnitude causes significant phase variation", "[impairments][phase_noise][parameters]") {
    PhaseNoiseImpairment pn(1000.0, 1.0, 1e6);
    const size_t N = 10000;
    std::vector<std::complex<float>> data(N, {1.0f, 0.0f});

    pn.apply(data.data(), N);

    float max_phase = 0.0f;
    for (size_t i = 0; i < N; ++i) {
        float phase = std::abs(std::arg(data[i]));
        max_phase = std::max(max_phase, phase);
    }
    REQUIRE(max_phase > 0.1f);
}

TEST_CASE("PhaseNoise different bandwidths produce different correlation", "[impairments][phase_noise][parameters]") {
    PhaseNoiseImpairment pn_narrow(10.0, 0.5, 1e6);
    PhaseNoiseImpairment pn_wide(100000.0, 0.5, 1e6);

    std::vector<std::complex<float>> data_narrow(1000, {1.0f, 0.0f});
    std::vector<std::complex<float>> data_wide(1000, {1.0f, 0.0f});

    pn_narrow.apply(data_narrow.data(), data_narrow.size());
    pn_wide.apply(data_wide.data(), data_wide.size());

    float phase_change_narrow = 0.0f;
    float phase_change_wide = 0.0f;
    for (size_t i = 1; i < 1000; ++i) {
        phase_change_narrow += std::abs(std::arg(data_narrow[i]) - std::arg(data_narrow[i - 1]));
        phase_change_wide += std::abs(std::arg(data_wide[i]) - std::arg(data_wide[i - 1]));
    }
    REQUIRE(phase_change_wide > phase_change_narrow);
}

TEST_CASE("PhaseNoise preserves magnitude regardless of parameters", "[impairments][phase_noise][parameters]") {
    PhaseNoiseImpairment pn(500.0, 2.0, 1e6);
    const size_t N = 1000;
    std::vector<std::complex<float>> data(N, {1.0f, 0.0f});

    pn.apply(data.data(), N);

    for (size_t i = 0; i < N; ++i) {
        REQUIRE_THAT(std::abs(data[i]), WithinAbs(1.0f, 1e-4f));
    }
}

TEST_CASE("PhaseNoise different PSD shapes work", "[impairments][phase_noise][parameters]") {
    PhaseNoiseImpairment pn1(100.0, 0.1, 1e6, "1f");
    PhaseNoiseImpairment pn2(100.0, 0.1, 1e6, "white");

    std::vector<std::complex<float>> data1(100, {1.0f, 0.0f});
    std::vector<std::complex<float>> data2(100, {1.0f, 0.0f});

    REQUIRE_NOTHROW(pn1.apply(data1.data(), data1.size()));
    REQUIRE_NOTHROW(pn2.apply(data2.data(), data2.size()));
}
