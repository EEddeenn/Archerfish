#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>

#include "archerfish/impairments/pa_nonlinearity.hpp"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("PaNonlinearity rejects invalid model", "[impairments][pa_nonlinearity]") {
    REQUIRE_THROWS_AS(
        archerfish::impairments::PaNonlinearityImpairment("invalid"),
        std::invalid_argument);
}

TEST_CASE("PaNonlinearity rejects non-positive saturation", "[impairments][pa_nonlinearity]") {
    REQUIRE_THROWS_AS(
        archerfish::impairments::PaNonlinearityImpairment("rapp", 0.0),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        archerfish::impairments::PaNonlinearityImpairment("rapp", -1.0),
        std::invalid_argument);
}

TEST_CASE("PaNonlinearity rejects non-positive smoothness", "[impairments][pa_nonlinearity]") {
    REQUIRE_THROWS_AS(
        archerfish::impairments::PaNonlinearityImpairment("rapp", 1.0, 0.0),
        std::invalid_argument);
}

TEST_CASE("PaNonlinearity name", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("rapp");
    REQUIRE(pa.name() == "pa_nonlinearity");
}

TEST_CASE("PaNonlinearity disabled preserves data", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("rapp", 1.0, 2.0, 0.5);
    std::vector<std::complex<float>> data = {{0.8f, 0.0f}, {0.0f, 0.6f}};
    auto original = data;

    pa.set_enabled(false);
    REQUIRE_FALSE(pa.enabled());
    pa.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(data[i] == original[i]);
    }
}

TEST_CASE("PaNonlinearity zero input produces zero output", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("rapp");
    std::vector<std::complex<float>> data = {{0.0f, 0.0f}, {0.0f, 0.0f}};

    pa.apply(data.data(), data.size());

    for (const auto& s : data) {
        REQUIRE_THAT(s.real(), WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(s.imag(), WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("PaNonlinearity Rapp low amplitude approximately linear", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("rapp", 1.0, 2.0, 0.0);
    float small_amp = 0.01f;
    std::vector<std::complex<float>> data = {{small_amp, 0.0f}};

    pa.apply(data.data(), data.size());

    float out_amp = std::abs(data[0]);
    REQUIRE_THAT(out_amp, WithinRel(small_amp, 0.01f));
}

TEST_CASE("PaNonlinearity Rapp high amplitude compressed", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("rapp", 1.0, 2.0, 0.0);
    float large_amp = 2.0f;
    std::vector<std::complex<float>> data = {{large_amp, 0.0f}};

    pa.apply(data.data(), data.size());

    float out_amp = std::abs(data[0]);
    REQUIRE(out_amp < large_amp);
    REQUIRE(out_amp <= 1.0f + 1e-5f);
}

TEST_CASE("PaNonlinearity Rapp AM/PM phase distortion", "[impairments][pa_nonlinearity]") {
    double phase_shift = 0.5;
    archerfish::impairments::PaNonlinearityImpairment pa("rapp", 1.0, 2.0, phase_shift);
    float amp = 0.5f;
    std::vector<std::complex<float>> data = {{amp, 0.0f}};

    pa.apply(data.data(), data.size());

    float expected_phase = static_cast<float>(phase_shift * amp / 1.0);
    float actual_phase = std::arg(data[0]);
    REQUIRE_THAT(actual_phase, WithinAbs(expected_phase, 1e-5f));
}

TEST_CASE("PaNonlinearity Saleh AM/AM compression at high input", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("saleh");
    float high_amp = 10.0f;
    std::vector<std::complex<float>> data = {{high_amp, 0.0f}};
    auto original_amp = std::abs(data[0]);

    pa.apply(data.data(), data.size());

    float out_amp = std::abs(data[0]);
    REQUIRE(out_amp < original_amp);
}

TEST_CASE("PaNonlinearity Saleh AM/PM phase distortion increases with amplitude", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("saleh");

    std::vector<std::complex<float>> data_low = {{0.1f, 0.0f}};
    std::vector<std::complex<float>> data_high = {{1.0f, 0.0f}};

    pa.apply(data_low.data(), data_low.size());
    pa.apply(data_high.data(), data_high.size());

    float phase_low = std::arg(data_low[0]);
    float phase_high = std::arg(data_high[0]);

    REQUIRE(std::abs(phase_high) > std::abs(phase_low));
}

TEST_CASE("PaNonlinearity Saleh low amplitude approximately linear gain", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("saleh");
    float small_amp = 0.01f;
    std::vector<std::complex<float>> data = {{small_amp, 0.0f}};

    pa.apply(data.data(), data.size());

    float out_amp = std::abs(data[0]);
    double a0 = 2.1587;
    float expected = static_cast<float>(a0 * small_amp);
    REQUIRE_THAT(out_amp, WithinRel(expected, 0.01f));
}

TEST_CASE("PaNonlinearity Rapp preserves phase of complex input", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("rapp", 1.0, 2.0, 0.0);
    float phase = static_cast<float>(M_PI / 4.0);
    float amp = 0.1f;
    std::vector<std::complex<float>> data = {
        std::polar(amp, phase)};

    pa.apply(data.data(), data.size());

    float out_phase = std::arg(data[0]);
    REQUIRE_THAT(out_phase, WithinAbs(phase, 1e-5f));
}

TEST_CASE("PaNonlinearity enabled property", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("rapp");
    REQUIRE(pa.enabled());
    pa.set_enabled(false);
    REQUIRE_FALSE(pa.enabled());
    pa.set_enabled(true);
    REQUIRE(pa.enabled());
}

TEST_CASE("PaNonlinearity Saleh zero phase at zero amplitude", "[impairments][pa_nonlinearity]") {
    archerfish::impairments::PaNonlinearityImpairment pa("saleh");
    std::vector<std::complex<float>> data = {{0.0f, 0.0f}};

    pa.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(0.0f, 1e-6f));
}
