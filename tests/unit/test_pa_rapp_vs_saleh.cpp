#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#include "archerfish/impairments/pa_nonlinearity.hpp"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace archerfish::impairments;

TEST_CASE("PA Rapp and Saleh produce different outputs for same input", "[impairments][pa][rapp_vs_saleh]") {
    PaNonlinearityImpairment rapp("rapp", 1.0, 2.0, 0.0);
    PaNonlinearityImpairment saleh("saleh");

    float input_amp = 0.5f;
    std::vector<std::complex<float>> data_r = {{input_amp, 0.0f}};
    std::vector<std::complex<float>> data_s = {{input_amp, 0.0f}};

    rapp.apply(data_r.data(), data_r.size());
    saleh.apply(data_s.data(), data_s.size());

    REQUIRE(std::abs(data_r[0].real() - data_s[0].real()) > 1e-4f);
}

TEST_CASE("PA Rapp with lower saturation compresses more", "[impairments][pa][rapp_vs_saleh]") {
    PaNonlinearityImpairment pa_low("rapp", 0.5, 2.0, 0.0);
    PaNonlinearityImpairment pa_high("rapp", 2.0, 2.0, 0.0);

    float input_amp = 0.8f;
    std::vector<std::complex<float>> data_low = {{input_amp, 0.0f}};
    std::vector<std::complex<float>> data_high = {{input_amp, 0.0f}};

    pa_low.apply(data_low.data(), data_low.size());
    pa_high.apply(data_high.data(), data_high.size());

    REQUIRE(std::abs(data_low[0]) < std::abs(data_high[0]));
}

TEST_CASE("PA Rapp with higher smoothness is more linear", "[impairments][pa][rapp_vs_saleh]") {
    PaNonlinearityImpairment pa_sharp("rapp", 1.0, 1.0, 0.0);
    PaNonlinearityImpairment pa_smooth("rapp", 1.0, 10.0, 0.0);

    float input_amp = 0.8f;
    std::vector<std::complex<float>> data_sharp = {{input_amp, 0.0f}};
    std::vector<std::complex<float>> data_smooth = {{input_amp, 0.0f}};

    pa_sharp.apply(data_sharp.data(), data_sharp.size());
    pa_smooth.apply(data_smooth.data(), data_smooth.size());

    float sharp_deviation = std::abs(std::abs(data_sharp[0]) - input_amp);
    float smooth_deviation = std::abs(std::abs(data_smooth[0]) - input_amp);
    REQUIRE(sharp_deviation > smooth_deviation);
}

TEST_CASE("PA both models handle zero input", "[impairments][pa][rapp_vs_saleh]") {
    PaNonlinearityImpairment rapp("rapp");
    PaNonlinearityImpairment saleh("saleh");

    std::vector<std::complex<float>> data_r = {{0.0f, 0.0f}};
    std::vector<std::complex<float>> data_s = {{0.0f, 0.0f}};

    rapp.apply(data_r.data(), data_r.size());
    saleh.apply(data_s.data(), data_s.size());

    REQUIRE_THAT(data_r[0].real(), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(data_s[0].real(), WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("PA Saleh low amplitude approximates linear gain a0", "[impairments][pa][rapp_vs_saleh]") {
    PaNonlinearityImpairment saleh("saleh");
    float small_amp = 0.01f;
    std::vector<std::complex<float>> data = {{small_amp, 0.0f}};

    saleh.apply(data.data(), data.size());

    float expected = static_cast<float>(2.1587 * small_amp);
    REQUIRE_THAT(std::abs(data[0]), WithinRel(expected, 0.01f));
}

TEST_CASE("PA rejects invalid numeric parameters", "[impairments][pa][rapp_vs_saleh]") {
    REQUIRE_THROWS_AS(PaNonlinearityImpairment("rapp", 0.0, 2.0, 0.0),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PaNonlinearityImpairment("rapp", 1.0, -1.0, 0.0),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PaNonlinearityImpairment("rapp", 1.0, 2.0,
                                               std::numeric_limits<double>::quiet_NaN()),
                      std::invalid_argument);
}
