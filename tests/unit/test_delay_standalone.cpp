#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <vector>

#include "archerfish/impairments/delay.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("Delay with 1 sample shifts correctly", "[impairments][delay][standalone]") {
    // 0.001 sec at 1000 sps = 1 sample delay (round(1.0) = 1)
    DelayImpairment delay(0.001, 1000.0);
    std::vector<std::complex<float>> data = {{10.0f, 0.0f}, {20.0f, 0.0f}, {30.0f, 0.0f}};

    delay.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[1].real(), WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(data[2].real(), WithinAbs(20.0f, 1e-5f));
}

TEST_CASE("Delay rounds fractional samples", "[impairments][delay][standalone]") {
    // 0.0015 sec at 1000 sps = 1.5 samples → rounds to 2
    DelayImpairment delay(0.0015, 1000.0);
    std::vector<std::complex<float>> data = {{10.0f, 0.0f}, {20.0f, 0.0f}, {30.0f, 0.0f}, {40.0f, 0.0f}};

    delay.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[1].real(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[2].real(), WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(data[3].real(), WithinAbs(20.0f, 1e-5f));
}

TEST_CASE("Delay on complex data preserves imaginary part", "[impairments][delay][standalone]") {
    DelayImpairment delay(0.001, 1000.0);  // 1 sample delay
    std::vector<std::complex<float>> data = {{1.0f, 10.0f}, {2.0f, 20.0f}, {3.0f, 30.0f}};

    delay.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[1].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data[1].imag(), WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(data[2].real(), WithinAbs(2.0f, 1e-5f));
    REQUIRE_THAT(data[2].imag(), WithinAbs(20.0f, 1e-5f));
}

TEST_CASE("Delay with single sample", "[impairments][delay][standalone]") {
    DelayImpairment delay(0.002, 1000.0);  // 2 samples delay
    std::vector<std::complex<float>> data = {{5.0f, 0.0f}};

    delay.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Delay multiple resets work correctly", "[impairments][delay][standalone]") {
    DelayImpairment delay(0.001, 1000.0);  // 1 sample delay

    // First batch
    std::vector<std::complex<float>> batch1 = {{10.0f, 0.0f}};
    delay.apply(batch1.data(), batch1.size());
    REQUIRE_THAT(batch1[0].real(), WithinAbs(0.0f, 1e-5f));

    // Reset
    delay.reset();

    // Second batch after reset - should also start with zeros
    std::vector<std::complex<float>> batch2 = {{20.0f, 0.0f}};
    delay.apply(batch2.data(), batch2.size());
    REQUIRE_THAT(batch2[0].real(), WithinAbs(0.0f, 1e-5f));

    // Third batch without reset - should get delayed value from batch2
    std::vector<std::complex<float>> batch3 = {{30.0f, 0.0f}};
    delay.apply(batch3.data(), batch3.size());
    REQUIRE_THAT(batch3[0].real(), WithinAbs(20.0f, 1e-5f));
}
