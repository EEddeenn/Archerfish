#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/amplitude_ripple.hpp"
#include "archerfish/impairments/burst_dropout.hpp"
#include "archerfish/impairments/delay.hpp"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("AmplitudeRipple zero depth preserves data", "[impairments][amplitude_ripple]") {
    archerfish::impairments::AmplitudeRippleImpairment ripple(0.0, 100.0, 1e6);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};
    auto original = data;

    ripple.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(original[i].real(), 1e-5f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(original[i].imag(), 1e-5f));
    }
}

TEST_CASE("AmplitudeRipple disabled preserves data", "[impairments][amplitude_ripple]") {
    archerfish::impairments::AmplitudeRippleImpairment ripple(0.5, 100.0, 1e6);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}, {0.0f, 1.0f}};
    auto original = data;

    ripple.set_enabled(false);
    REQUIRE_FALSE(ripple.enabled());
    ripple.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(data[i] == original[i]);
    }
}

TEST_CASE("AmplitudeRipple applies sinusoidal variation", "[impairments][amplitude_ripple]") {
    archerfish::impairments::AmplitudeRippleImpairment ripple(0.5, 1000.0, 10000.0);
    std::vector<std::complex<float>> data(1000, {1.0f, 0.0f});

    ripple.apply(data.data(), data.size());

    bool has_greater = false;
    bool has_lesser = false;
    for (const auto& s : data) {
        float mag = std::abs(s);
        if (mag > 1.001f) has_greater = true;
        if (mag < 0.999f) has_lesser = true;
    }
    REQUIRE(has_greater);
    REQUIRE(has_lesser);
}

TEST_CASE("AmplitudeRipple phase continuity across calls", "[impairments][amplitude_ripple]") {
    double sample_rate = 10000.0;
    double freq = 1000.0;
    double depth = 0.5;
    archerfish::impairments::AmplitudeRippleImpairment ripple_separate(depth, freq, sample_rate);
    archerfish::impairments::AmplitudeRippleImpairment ripple_combined(depth, freq, sample_rate);

    std::vector<std::complex<float>> data1(50, {1.0f, 0.0f});
    std::vector<std::complex<float>> data2(50, {1.0f, 0.0f});
    ripple_separate.apply(data1.data(), data1.size());
    ripple_separate.apply(data2.data(), data2.size());

    std::vector<std::complex<float>> combined(100, {1.0f, 0.0f});
    ripple_combined.apply(combined.data(), combined.size());

    for (size_t i = 0; i < 50; ++i) {
        REQUIRE_THAT(combined[i].real(), WithinAbs(data1[i].real(), 1e-5f));
        REQUIRE_THAT(combined[i].imag(), WithinAbs(data1[i].imag(), 1e-5f));
    }
    for (size_t i = 0; i < 50; ++i) {
        REQUIRE_THAT(combined[50 + i].real(), WithinAbs(data2[i].real(), 1e-5f));
        REQUIRE_THAT(combined[50 + i].imag(), WithinAbs(data2[i].imag(), 1e-5f));
    }
}

TEST_CASE("AmplitudeRipple name", "[impairments][amplitude_ripple]") {
    archerfish::impairments::AmplitudeRippleImpairment ripple;
    REQUIRE(ripple.name() == "amplitude_ripple");
}

TEST_CASE("Delay zero delay preserves data", "[impairments][delay]") {
    archerfish::impairments::DelayImpairment delay(0.0, 1e6);
    std::vector<std::complex<float>> data = {{1.0f, 2.0f}, {3.0f, 4.0f}};
    auto original = data;

    delay.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(data[i] == original[i]);
    }
}

TEST_CASE("Delay shifts samples by N", "[impairments][delay]") {
    size_t delay_n = 3;
    double sample_rate = 1000.0;
    archerfish::impairments::DelayImpairment delay(static_cast<double>(delay_n) / sample_rate, sample_rate);

    std::vector<std::complex<float>> data = {{1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}, {4.0f, 0.0f}, {5.0f, 0.0f}};
    delay.apply(data.data(), data.size());

    for (size_t i = 0; i < delay_n; ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(0.0f, 1e-5f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(0.0f, 1e-5f));
    }
    REQUIRE_THAT(data[3].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data[4].real(), WithinAbs(2.0f, 1e-5f));
}

TEST_CASE("Delay persists across multiple apply calls", "[impairments][delay]") {
    size_t delay_n = 2;
    double sample_rate = 1000.0;
    archerfish::impairments::DelayImpairment delay(static_cast<double>(delay_n) / sample_rate, sample_rate);

    std::vector<std::complex<float>> chunk1 = {{10.0f, 0.0f}, {20.0f, 0.0f}};
    delay.apply(chunk1.data(), chunk1.size());

    for (size_t i = 0; i < chunk1.size(); ++i) {
        REQUIRE_THAT(chunk1[i].real(), WithinAbs(0.0f, 1e-5f));
    }

    std::vector<std::complex<float>> chunk2 = {{30.0f, 0.0f}, {40.0f, 0.0f}};
    delay.apply(chunk2.data(), chunk2.size());

    REQUIRE_THAT(chunk2[0].real(), WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(chunk2[1].real(), WithinAbs(20.0f, 1e-5f));
}

TEST_CASE("Delay reset clears buffer", "[impairments][delay]") {
    size_t delay_n = 3;
    double sample_rate = 1000.0;
    archerfish::impairments::DelayImpairment delay(static_cast<double>(delay_n) / sample_rate, sample_rate);

    std::vector<std::complex<float>> chunk1 = {{1.0f, 0.0f}, {2.0f, 0.0f}};
    delay.apply(chunk1.data(), chunk1.size());

    delay.reset();

    std::vector<std::complex<float>> chunk2 = {{10.0f, 0.0f}, {20.0f, 0.0f}, {30.0f, 0.0f}, {40.0f, 0.0f}};
    delay.apply(chunk2.data(), chunk2.size());

    for (size_t i = 0; i < delay_n; ++i) {
        REQUIRE_THAT(chunk2[i].real(), WithinAbs(0.0f, 1e-5f));
    }
    REQUIRE_THAT(chunk2[3].real(), WithinAbs(10.0f, 1e-5f));
}

TEST_CASE("Delay name", "[impairments][delay]") {
    archerfish::impairments::DelayImpairment delay;
    REQUIRE(delay.name() == "delay");
}

TEST_CASE("Delay disable preserves data", "[impairments][delay]") {
    archerfish::impairments::DelayImpairment delay(0.001, 1e6);
    std::vector<std::complex<float>> data = {{1.0f, 2.0f}};
    auto original = data;

    delay.set_enabled(false);
    REQUIRE_FALSE(delay.enabled());
    delay.apply(data.data(), data.size());

    REQUIRE(data[0] == original[0]);
}

TEST_CASE("BurstDropout zero rate preserves data", "[impairments][burst_dropout]") {
    archerfish::impairments::BurstDropoutImpairment burst(0.0, 10.0, 42);
    std::vector<std::complex<float>> data(1000, {1.0f, 0.0f});
    auto original = data;

    burst.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(data[i] == original[i]);
    }
}

TEST_CASE("BurstDropout disabled preserves data", "[impairments][burst_dropout]") {
    archerfish::impairments::BurstDropoutImpairment burst(0.5, 10.0, 42);
    std::vector<std::complex<float>> data(100, {1.0f, 0.0f});
    auto original = data;

    burst.set_enabled(false);
    REQUIRE_FALSE(burst.enabled());
    burst.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(data[i] == original[i]);
    }
}

TEST_CASE("BurstDropout high rate drops some samples", "[impairments][burst_dropout]") {
    archerfish::impairments::BurstDropoutImpairment burst(0.5, 10.0, 42);
    std::vector<std::complex<float>> data(10000, {1.0f, 0.0f});

    burst.apply(data.data(), data.size());

    size_t zeros = 0;
    for (const auto& s : data) {
        if (s == std::complex<float>(0.0f, 0.0f)) {
            ++zeros;
        }
    }
    REQUIRE(zeros > 0);
    REQUIRE(zeros < data.size());
}

TEST_CASE("BurstDropout seeded reproducibility", "[impairments][burst_dropout]") {
    std::vector<std::complex<float>> data1(1000, {1.0f, 0.0f});
    std::vector<std::complex<float>> data2(1000, {1.0f, 0.0f});

    archerfish::impairments::BurstDropoutImpairment burst1(0.3, 5.0, 12345);
    archerfish::impairments::BurstDropoutImpairment burst2(0.3, 5.0, 12345);

    burst1.apply(data1.data(), data1.size());
    burst2.apply(data2.data(), data2.size());

    for (size_t i = 0; i < data1.size(); ++i) {
        REQUIRE(data1[i] == data2[i]);
    }
}

TEST_CASE("BurstDropout signal resumes after burst", "[impairments][burst_dropout]") {
    archerfish::impairments::BurstDropoutImpairment burst(0.3, 3.0, 99);
    std::vector<std::complex<float>> data(10000, {1.0f, 0.0f});

    burst.apply(data.data(), data.size());

    bool found_nonzero_after_zero = false;
    bool seen_zero = false;
    for (const auto& s : data) {
        if (s == std::complex<float>(0.0f, 0.0f)) {
            seen_zero = true;
        } else if (seen_zero) {
            found_nonzero_after_zero = true;
            break;
        }
    }
    REQUIRE(found_nonzero_after_zero);
}

TEST_CASE("BurstDropout name", "[impairments][burst_dropout]") {
    archerfish::impairments::BurstDropoutImpairment burst;
    REQUIRE(burst.name() == "burst_dropout");
}
