#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <vector>

#include "archerfish/impairments/dc_offset.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("DCOffset zero values is identity", "[impairments][dc_offset][standalone]") {
    DcOffsetImpairment dc(0.0, 0.0);
    std::vector<std::complex<float>> data = {{1.0f, 2.0f}, {-0.5f, 0.3f}};
    auto original = data;

    dc.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(original[i].real(), 1e-6f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(original[i].imag(), 1e-6f));
    }
}

TEST_CASE("DCOffset negative values subtract", "[impairments][dc_offset][standalone]") {
    DcOffsetImpairment dc(-0.1, -0.2);
    std::vector<std::complex<float>> data(5, {1.0f, 1.0f});

    dc.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(0.9f, 1e-5f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(0.8f, 1e-5f));
    }
}

TEST_CASE("DCOffset on zero data adds constant", "[impairments][dc_offset][standalone]") {
    DcOffsetImpairment dc(0.05, -0.03);
    std::vector<std::complex<float>> data(10, {0.0f, 0.0f});

    dc.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(0.05f, 1e-5f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(-0.03f, 1e-5f));
    }
}

TEST_CASE("DCOffset asymmetric I/Q offsets", "[impairments][dc_offset][standalone]") {
    DcOffsetImpairment dc(0.5, 0.0);
    std::vector<std::complex<float>> data = {{0.0f, 0.0f}};

    dc.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(0.5f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("DCOffset default constructor is enabled", "[impairments][dc_offset][standalone]") {
    DcOffsetImpairment dc;
    REQUIRE(dc.enabled());
    REQUIRE(dc.name() == "dc_offset");
}
