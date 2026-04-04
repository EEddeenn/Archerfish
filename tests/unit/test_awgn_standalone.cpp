#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/awgn.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("AWGN zero power is transparent", "[impairments][awgn][standalone]") {
    AwgnImpairment awgn(0.0, 42);
    std::vector<std::complex<float>> data = {{1.0f, 0.5f}, {0.3f, 0.7f}, {2.0f, -1.0f}};
    auto original = data;

    awgn.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(data[i] == original[i]);
    }
}

TEST_CASE("AWGN seeded reproducibility", "[impairments][awgn][standalone]") {
    const size_t N = 1000;
    std::vector<std::complex<float>> data1(N, {1.0f, 0.0f});
    std::vector<std::complex<float>> data2(N, {1.0f, 0.0f});

    AwgnImpairment awgn1(0.1, 99999);
    AwgnImpairment awgn2(0.1, 99999);

    awgn1.apply(data1.data(), N);
    awgn2.apply(data2.data(), N);

    for (size_t i = 0; i < N; ++i) {
        REQUIRE(data1[i].real() == data2[i].real());
        REQUIRE(data1[i].imag() == data2[i].imag());
    }
}

TEST_CASE("AWGN different seeds produce different noise", "[impairments][awgn][standalone]") {
    const size_t N = 1000;
    std::vector<std::complex<float>> data1(N, {0.0f, 0.0f});
    std::vector<std::complex<float>> data2(N, {0.0f, 0.0f});

    AwgnImpairment awgn1(1.0, 11111);
    AwgnImpairment awgn2(1.0, 22222);

    awgn1.apply(data1.data(), N);
    awgn2.apply(data2.data(), N);

    bool different = false;
    for (size_t i = 0; i < N; ++i) {
        if (data1[i] != data2[i]) {
            different = true;
            break;
        }
    }
    REQUIRE(different);
}

TEST_CASE("AWGN higher power yields higher variance", "[impairments][awgn][standalone]") {
    const size_t N = 50000;

    std::vector<std::complex<float>> data_low(N, {0.0f, 0.0f});
    std::vector<std::complex<float>> data_high(N, {0.0f, 0.0f});

    AwgnImpairment awgn_low(0.01, 42);
    AwgnImpairment awgn_high(1.0, 42);

    awgn_low.apply(data_low.data(), N);
    awgn_high.apply(data_high.data(), N);

    auto variance = [](const std::vector<std::complex<float>>& d) {
        double sum = 0.0;
        for (const auto& s : d) {
            double mag = std::abs(s);
            sum += mag * mag;
        }
        return sum / static_cast<double>(d.size());
    };

    double var_low = variance(data_low);
    double var_high = variance(data_high);

    REQUIRE(var_high > var_low);
}

TEST_CASE("AWGN default constructor is enabled", "[impairments][awgn][standalone]") {
    AwgnImpairment awgn;
    REQUIRE(awgn.enabled());
    REQUIRE(awgn.name() == "awgn");
}
