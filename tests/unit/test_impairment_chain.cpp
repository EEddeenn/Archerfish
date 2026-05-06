#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <memory>
#include <stdexcept>
#include <vector>

#include "archerfish/impairments/impairment_chain.hpp"
#include "archerfish/impairments/dc_offset.hpp"
#include "archerfish/impairments/phase_offset.hpp"
#include "archerfish/impairments/cfo.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("Empty chain does nothing", "[impairment_chain]") {
    archerfish::impairments::ImpairmentChain chain;
    std::vector<std::complex<float>> data = {{1.0f, 2.0f}, {3.0f, 4.0f}};
    auto original = data;

    chain.apply(data.data(), data.size());

    REQUIRE(data == original);
}

TEST_CASE("Chain validates null data for nonzero work", "[impairment_chain]") {
    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(1.0, 0.0));

    REQUIRE_NOTHROW(chain.apply(nullptr, 0));
    REQUIRE_THROWS_AS(chain.apply(nullptr, 1), std::invalid_argument);
}

TEST_CASE("Chain with one impairment applies it", "[impairment_chain]") {
    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(1.0, 2.0));

    std::vector<std::complex<float>> data = {{0.0f, 0.0f}};
    chain.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(2.0f, 1e-5f));
}

TEST_CASE("Chain applies impairments in order", "[impairment_chain]") {
    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(1.0, 0.0));
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(0.0, 1.0));

    std::vector<std::complex<float>> data = {{0.0f, 0.0f}};
    chain.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Chain enable/disable individual", "[impairment_chain]") {
    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(1.0, 0.0));
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(0.0, 1.0));

    chain.set_enabled(0, false);

    std::vector<std::complex<float>> data = {{0.0f, 0.0f}};
    chain.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Chain size and access", "[impairment_chain]") {
    archerfish::impairments::ImpairmentChain chain;
    REQUIRE(chain.size() == 0);

    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(1.0, 0.0));
    chain.add(std::make_unique<archerfish::impairments::PhaseOffsetImpairment>(0.5));

    REQUIRE(chain.size() == 2);
    REQUIRE(chain.at(0).name() == "dc_offset");
    REQUIRE(chain.at(1).name() == "phase_offset");
}

TEST_CASE("Chain clear", "[impairment_chain]") {
    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(1.0, 0.0));
    chain.add(std::make_unique<archerfish::impairments::PhaseOffsetImpairment>(0.5));
    REQUIRE(chain.size() == 2);

    chain.clear();
    REQUIRE(chain.size() == 0);

    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};
    auto original = data;
    chain.apply(data.data(), data.size());
    REQUIRE(data == original);
}

TEST_CASE("Chain out of range throws", "[impairment_chain]") {
    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(1.0, 0.0));

    REQUIRE_THROWS_AS(chain.at(5), std::out_of_range);
}

TEST_CASE("Chain combine CFO and DC offset", "[impairment_chain]") {
    archerfish::impairments::ImpairmentChain chain;
    chain.add(std::make_unique<archerfish::impairments::CfoImpairment>(0.0, 1e6));
    chain.add(std::make_unique<archerfish::impairments::DcOffsetImpairment>(0.5, -0.3));

    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};
    chain.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.5f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(-0.3f, 1e-5f));
}
