#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/common/constants.hpp"
#include "archerfish/impairments/phase_offset.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("PhaseOffset 2pi is identity", "[impairments][phase_offset][standalone]") {
    PhaseOffsetImpairment po(2.0 * archerfish::constants::kPi);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}, {0.0f, 1.0f}, {0.5f, -0.5f}};

    auto original = data;
    po.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(original[i].real(), 1e-5f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(original[i].imag(), 1e-5f));
    }
}

TEST_CASE("PhaseOffset pi flips sign", "[impairments][phase_offset][standalone]") {
    PhaseOffsetImpairment po(archerfish::constants::kPi);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};

    po.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(-1.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("PhaseOffset negative angle rotates clockwise", "[impairments][phase_offset][standalone]") {
    PhaseOffsetImpairment po(-archerfish::constants::kPi / 2.0);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};

    po.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(-1.0f, 1e-5f));
}

TEST_CASE("PhaseOffset preserves magnitude", "[impairments][phase_offset][standalone]") {
    PhaseOffsetImpairment po(1.234);
    std::vector<std::complex<float>> data = {{3.0f, 4.0f}};
    float original_mag = std::abs(data[0]);

    po.apply(data.data(), data.size());

    REQUIRE_THAT(std::abs(data[0]), WithinAbs(original_mag, 1e-5f));
}

TEST_CASE("PhaseOffset idempotent on same data", "[impairments][phase_offset][standalone]") {
    PhaseOffsetImpairment po(0.7);
    std::vector<std::complex<float>> data1 = {{1.0f, 1.0f}};
    std::vector<std::complex<float>> data2 = {{1.0f, 1.0f}};

    po.apply(data1.data(), data1.size());
    po.apply(data2.data(), data2.size());

    REQUIRE_THAT(data1[0].real(), WithinAbs(data2[0].real(), 1e-6f));
    REQUIRE_THAT(data1[0].imag(), WithinAbs(data2[0].imag(), 1e-6f));
}
