#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/impairment_chain.hpp"
#include "archerfish/impairments/cfo.hpp"
#include "archerfish/impairments/dc_offset.hpp"
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
