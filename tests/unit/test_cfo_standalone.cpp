#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/cfo.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("CFO negative frequency rotates opposite direction", "[impairments][cfo][standalone]") {
    double sample_rate = 4.0;
    // Negative CFO = -1.0 Hz, so at sample 1 the phase advances by -2π/4 = -π/2
    CfoImpairment cfo(-1.0, sample_rate);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}, {1.0f, 0.0f}};

    cfo.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(0.0f, 1e-5f));
    // Sample 1: phase = -π/2, so cos(-π/2)=0, sin(-π/2)=-1
    REQUIRE_THAT(data[1].real(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[1].imag(), WithinAbs(-1.0f, 1e-5f));
}

TEST_CASE("CFO at Nyquist rate alternates sign", "[impairments][cfo][standalone]") {
    double sample_rate = 1.0;
    // CFO = 0.5 Hz (Nyquist), so phase advance per sample = 2π * 0.5 / 1.0 = π
    CfoImpairment cfo(0.5, sample_rate);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}};

    cfo.apply(data.data(), data.size());

    // Sample 0: phase=0, cos(0)=1
    REQUIRE_THAT(data[0].real(), WithinAbs(1.0f, 1e-5f));
    // Sample 1: phase=π, cos(π)=-1
    REQUIRE_THAT(data[1].real(), WithinAbs(-1.0f, 1e-5f));
    // Sample 2: phase=2π, cos(2π)=1
    REQUIRE_THAT(data[2].real(), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("CFO phase continuity across many small batches", "[impairments][cfo][standalone]") {
    double sample_rate = 100.0;
    double cfo_hz = 10.0;
    CfoImpairment cfo_batched(cfo_hz, sample_rate);
    CfoImpairment cfo_single(cfo_hz, sample_rate);

    // Apply 10 batches of 10 samples each
    std::vector<std::complex<float>> batched_result(100, {1.0f, 0.0f});
    for (size_t b = 0; b < 10; ++b) {
        std::vector<std::complex<float>> chunk(10, {1.0f, 0.0f});
        cfo_batched.apply(chunk.data(), chunk.size());
        for (size_t i = 0; i < 10; ++i) {
            batched_result[b * 10 + i] = chunk[i];
        }
    }

    // Apply all 100 samples at once
    std::vector<std::complex<float>> single_result(100, {1.0f, 0.0f});
    cfo_single.apply(single_result.data(), single_result.size());

    for (size_t i = 0; i < 100; ++i) {
        REQUIRE_THAT(batched_result[i].real(), WithinAbs(single_result[i].real(), 1e-4f));
        REQUIRE_THAT(batched_result[i].imag(), WithinAbs(single_result[i].imag(), 1e-4f));
    }
}

TEST_CASE("CFO default constructor is enabled", "[impairments][cfo][standalone]") {
    CfoImpairment cfo;
    REQUIRE(cfo.enabled());
    REQUIRE(cfo.name() == "cfo");
}

TEST_CASE("CFO on zero-valued data produces zero", "[impairments][cfo][standalone]") {
    CfoImpairment cfo(1000.0, 1e6);
    std::vector<std::complex<float>> data(10, {0.0f, 0.0f});

    cfo.apply(data.data(), data.size());

    for (const auto& s : data) {
        REQUIRE_THAT(s.real(), WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(s.imag(), WithinAbs(0.0f, 1e-6f));
    }
}
