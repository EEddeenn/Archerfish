#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/iq_imbalance.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("IQImbalance phase imbalance creates cross-talk", "[impairments][iq_imbalance][standalone]") {
    // With phase imbalance, I and Q get mixed
    IqImbalanceImpairment iq(0.0, 0.5);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};

    iq.apply(data.data(), data.size());

    // With phase imbalance, Q component should now be non-zero for purely I input
    REQUIRE(std::abs(data[0].imag()) > 1e-6f);
}

TEST_CASE("IQImbalance small values approximately transparent", "[impairments][iq_imbalance][standalone]") {
    IqImbalanceImpairment iq(0.001, 0.001);
    std::vector<std::complex<float>> data = {{1.0f, 1.0f}, {0.5f, -0.3f}};
    auto original = data;

    iq.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(original[i].real(), 0.01f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(original[i].imag(), 0.01f));
    }
}

TEST_CASE("IQImbalance on purely imaginary input", "[impairments][iq_imbalance][standalone]") {
    IqImbalanceImpairment iq(6.0, 0.0);
    std::vector<std::complex<float>> data = {{0.0f, 1.0f}};

    iq.apply(data.data(), data.size());

    // With gain imbalance, Q should be attenuated
    float q = data[0].imag();
    REQUIRE(q < 1.0f);
}

TEST_CASE("IQImbalance zero input stays zero", "[impairments][iq_imbalance][standalone]") {
    IqImbalanceImpairment iq(10.0, 1.0);
    std::vector<std::complex<float>> data = {{0.0f, 0.0f}, {0.0f, 0.0f}};

    iq.apply(data.data(), data.size());

    for (const auto& s : data) {
        REQUIRE_THAT(s.real(), WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(s.imag(), WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("IQImbalance default constructor is enabled", "[impairments][iq_imbalance][standalone]") {
    IqImbalanceImpairment iq;
    REQUIRE(iq.enabled());
    REQUIRE(iq.name() == "iq_imbalance");
}
