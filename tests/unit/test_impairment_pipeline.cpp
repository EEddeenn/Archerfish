#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#include "archerfish/impairments/impairment_chain.hpp"
#include "archerfish/impairments/amplitude_ripple.hpp"
#include "archerfish/impairments/cfo.hpp"
#include "archerfish/impairments/burst_dropout.hpp"
#include "archerfish/impairments/dc_offset.hpp"
#include "archerfish/impairments/delay.hpp"
#include "archerfish/impairments/fading.hpp"
#include "archerfish/impairments/iq_imbalance.hpp"
#include "archerfish/impairments/multipath.hpp"
#include "archerfish/impairments/pa_nonlinearity.hpp"
#include "archerfish/impairments/phase_noise.hpp"
#include "archerfish/impairments/phase_offset.hpp"
#include "archerfish/impairments/awgn.hpp"
#include "archerfish/scenario/scenario.hpp"

using namespace archerfish::impairments;
using Catch::Matchers::WithinAbs;

TEST_CASE("Impairment pipeline: build_chain from settings with CFO", "[integration][impairment]") {
    archerfish::scenario::ImpairmentSettings settings;
    settings.cfo_hz = 100.0;
    auto chain = build_chain(settings, 1e6);
    REQUIRE(chain != nullptr);
    REQUIRE(chain->size() == 1);
    CHECK(chain->at(0).name() == "cfo");
}

TEST_CASE("Impairment pipeline: build_chain with multiple impairments", "[integration][impairment]") {
    archerfish::scenario::ImpairmentSettings settings;
    settings.cfo_hz = 100.0;
    settings.dc_offset_i = 0.01;
    settings.dc_offset_q = -0.02;
    settings.phase_offset_rad = 0.1;
    settings.awgn_power = 0.001;
    auto chain = build_chain(settings, 1e6);
    REQUIRE(chain != nullptr);
    CHECK(chain->size() == 4);
}

TEST_CASE("Impairment pipeline: build_chain with empty settings returns nullptr", "[integration][impairment]") {
    archerfish::scenario::ImpairmentSettings settings;
    auto chain = build_chain(settings, 1e6);
    CHECK(chain == nullptr);
}

TEST_CASE("Impairment pipeline: chain modifies samples", "[integration][impairment]") {
    archerfish::scenario::ImpairmentSettings settings;
    settings.dc_offset_i = 1.0;
    settings.dc_offset_q = 1.0;
    auto chain = build_chain(settings, 1e6);
    REQUIRE(chain != nullptr);

    std::vector<std::complex<float>> data = {{0.0f, 0.0f}, {0.5f, 0.5f}};
    auto original = data;
    chain->apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Impairment pipeline: chain enable/disable toggles effect", "[integration][impairment]") {
    archerfish::scenario::ImpairmentSettings settings;
    settings.dc_offset_i = 5.0;
    settings.dc_offset_q = 0.0;
    auto chain = build_chain(settings, 1e6);
    REQUIRE(chain != nullptr);

    chain->set_enabled(0, false);
    std::vector<std::complex<float>> data = {{0.0f, 0.0f}};
    chain->apply(data.data(), data.size());
    REQUIRE_THAT(data[0].real(), WithinAbs(0.0f, 1e-5f));

    chain->set_enabled(0, true);
    chain->apply(data.data(), data.size());
    REQUIRE_THAT(data[0].real(), WithinAbs(5.0f, 1e-5f));
}

TEST_CASE("Impairment pipeline rejects invalid build settings", "[integration][impairment]") {
    archerfish::scenario::ImpairmentSettings invalid_sample_rate;
    invalid_sample_rate.cfo_hz = 100.0;
    REQUIRE_THROWS_AS(build_chain(invalid_sample_rate, 0.0), std::invalid_argument);

    archerfish::scenario::ImpairmentSettings negative_multipath_delay;
    negative_multipath_delay.multipath_delay_samples = -1.0;
    negative_multipath_delay.multipath_amplitude = 0.5;
    REQUIRE_THROWS_AS(build_chain(negative_multipath_delay, 1e6), std::invalid_argument);

    archerfish::scenario::ImpairmentSettings fractional_multipath_delay;
    fractional_multipath_delay.multipath_delay_samples = 1.5;
    fractional_multipath_delay.multipath_amplitude = 0.5;
    REQUIRE_THROWS_AS(build_chain(fractional_multipath_delay, 1e6), std::invalid_argument);
}

TEST_CASE("Impairments reject null apply buffers for non-zero counts", "[integration][impairment]") {
    std::complex<float>* null_data = nullptr;

    CHECK_NOTHROW(ImpairmentChain{}.apply(null_data, 0));
    CHECK_THROWS_AS(ImpairmentChain{}.apply(null_data, 1), std::invalid_argument);

    CHECK_NOTHROW(AwgnImpairment{}.apply(null_data, 0));
    CHECK_THROWS_AS(AwgnImpairment{}.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(CfoImpairment{}.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(PhaseOffsetImpairment{}.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(DcOffsetImpairment{}.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(IqImbalanceImpairment{}.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(AmplitudeRippleImpairment{}.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(DelayImpairment{}.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(BurstDropoutImpairment{}.apply(null_data, 1), std::invalid_argument);

    PhaseNoiseImpairment phase_noise(100.0, 0.1, 1e6);
    MultipathImpairment multipath(1, 0.5f);
    FadingImpairment fading(100.0, 1e6);
    PaNonlinearityImpairment pa("rapp");
    CHECK_THROWS_AS(phase_noise.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(multipath.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(fading.apply(null_data, 1), std::invalid_argument);
    CHECK_THROWS_AS(pa.apply(null_data, 1), std::invalid_argument);
}
