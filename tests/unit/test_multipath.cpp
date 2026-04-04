#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <vector>

#include "archerfish/impairments/multipath.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("Multipath adds delayed copy") {
    MultipathImpairment mp(2, 0.5f);
    std::vector<std::complex<float>> data = {
        {1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};

    mp.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(data[0].imag(), WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(data[1].real(), WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(data[2].real(), WithinAbs(0.5, 1e-6));
    REQUIRE_THAT(data[3].real(), WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(data[4].real(), WithinAbs(0.0, 1e-6));
}

TEST_CASE("Multipath with zero amplitude is transparent") {
    MultipathImpairment mp(5, 0.0f);
    std::vector<std::complex<float>> data = {
        {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}};

    mp.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(data[1].real(), WithinAbs(2.0, 1e-6));
    REQUIRE_THAT(data[2].real(), WithinAbs(3.0, 1e-6));
}

TEST_CASE("Multipath name is correct") {
    MultipathImpairment mp(10, 0.5f);
    REQUIRE(mp.name() == "multipath");
}

TEST_CASE("Multipath enabled/disabled") {
    MultipathImpairment mp(3, 0.5f);
    mp.set_enabled(false);

    std::vector<std::complex<float>> data = {
        {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}, {4.0f, 0.0f}};

    mp.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(data[1].real(), WithinAbs(2.0, 1e-6));
    REQUIRE_THAT(data[2].real(), WithinAbs(3.0, 1e-6));
    REQUIRE_THAT(data[3].real(), WithinAbs(4.0, 1e-6));
}

TEST_CASE("Multipath with zero delay is transparent") {
    MultipathImpairment mp(0, 0.5f);
    std::vector<std::complex<float>> data = {
        {1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}};

    mp.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(data[1].real(), WithinAbs(2.0, 1e-6));
    REQUIRE_THAT(data[2].real(), WithinAbs(3.0, 1e-6));
}

TEST_CASE("Multipath applies across multiple calls") {
    MultipathImpairment mp(2, 1.0f);

    std::vector<std::complex<float>> batch1 = {{1.0f, 0.0f}, {0.0f, 0.0f}};
    mp.apply(batch1.data(), batch1.size());

    REQUIRE_THAT(batch1[0].real(), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(batch1[1].real(), WithinAbs(0.0, 1e-6));

    std::vector<std::complex<float>> batch2 = {{0.0f, 0.0f}, {0.0f, 0.0f}};
    mp.apply(batch2.data(), batch2.size());

    REQUIRE_THAT(batch2[0].real(), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(batch2[1].real(), WithinAbs(0.0, 1e-6));
}

TEST_CASE("Multipath with complex values") {
    MultipathImpairment mp(1, 0.5f);
    std::vector<std::complex<float>> data = {
        {1.0f, 1.0f}, {2.0f, -1.0f}, {0.0f, 3.0f}};

    mp.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(data[0].imag(), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(data[1].real(), WithinAbs(2.5, 1e-6));
    REQUIRE_THAT(data[1].imag(), WithinAbs(-0.5, 1e-6));
    REQUIRE_THAT(data[2].real(), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(data[2].imag(), WithinAbs(2.5, 1e-6));
}
