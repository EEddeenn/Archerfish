#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <vector>

#include "archerfish/impairments/multipath.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("Multipath with large delay adds far copy", "[impairments][multipath][taps]") {
    MultipathImpairment mp(10, 0.5f);
    std::vector<std::complex<float>> data = {
        {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}, {4.0f, 0.0f}, {5.0f, 0.0f},
        {6.0f, 0.0f}, {7.0f, 0.0f}, {8.0f, 0.0f}, {9.0f, 0.0f}, {10.0f, 0.0f},
        {11.0f, 0.0f}, {12.0f, 0.0f}};

    mp.apply(data.data(), data.size());

    for (size_t i = 0; i < 10; ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(static_cast<float>(i + 1), 1e-6));
    }
    REQUIRE_THAT(data[10].real(), WithinAbs(11.5f, 1e-6));
    // data[11] = original 12 + echo of history[1] which was data[1]=2 → 12 + 0.5*2 = 13.0
    REQUIRE_THAT(data[11].real(), WithinAbs(13.0f, 1e-6));
}

TEST_CASE("Multipath with very small amplitude nearly transparent", "[impairments][multipath][taps]") {
    MultipathImpairment mp(3, 0.001f);
    std::vector<std::complex<float>> data = {
        {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}, {4.0f, 0.0f}, {5.0f, 0.0f}};
    auto original = data;

    mp.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(original[i].real(), 0.01f));
    }
}

TEST_CASE("Multipath with unit amplitude doubles signal at delay point", "[impairments][multipath][taps]") {
    MultipathImpairment mp(2, 1.0f);
    std::vector<std::complex<float>> data = {
        {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}, {4.0f, 0.0f}};

    mp.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0f, 1e-6));
    REQUIRE_THAT(data[1].real(), WithinAbs(2.0f, 1e-6));
    REQUIRE_THAT(data[2].real(), WithinAbs(4.0f, 1e-6));
    REQUIRE_THAT(data[3].real(), WithinAbs(6.0f, 1e-6));
}

TEST_CASE("Multipath delay=1 with amplitude", "[impairments][multipath][taps]") {
    MultipathImpairment mp(1, 0.5f);
    std::vector<std::complex<float>> data = {
        {2.0f, 0.0f}, {4.0f, 0.0f}, {6.0f, 0.0f}};

    mp.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(2.0f, 1e-6));
    REQUIRE_THAT(data[1].real(), WithinAbs(5.0f, 1e-6));
    REQUIRE_THAT(data[2].real(), WithinAbs(8.0f, 1e-6));
}

TEST_CASE("Multipath state persists across multiple apply calls with large delay", "[impairments][multipath][taps]") {
    MultipathImpairment mp(5, 1.0f);

    std::vector<std::complex<float>> batch1(5, {1.0f, 0.0f});
    mp.apply(batch1.data(), batch1.size());

    for (size_t i = 0; i < 5; ++i) {
        REQUIRE_THAT(batch1[i].real(), WithinAbs(1.0f, 1e-6));
    }

    std::vector<std::complex<float>> batch2(5, {0.0f, 0.0f});
    mp.apply(batch2.data(), batch2.size());

    REQUIRE_THAT(batch2[0].real(), WithinAbs(1.0f, 1e-6));
}
