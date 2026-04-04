#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <memory>
#include <vector>

#include "archerfish/impairments/impairment_chain.hpp"
#include "archerfish/impairments/dc_offset.hpp"
#include "archerfish/impairments/phase_offset.hpp"
#include "archerfish/impairments/cfo.hpp"
#include "archerfish/impairments/awgn.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("Chain ordering: DC offset then phase offset", "[impairment_chain][ordering]") {
    ImpairmentChain chain;
    chain.add(std::make_unique<DcOffsetImpairment>(1.0, 0.0));
    chain.add(std::make_unique<PhaseOffsetImpairment>(0.0));

    std::vector<std::complex<float>> data = {{0.0f, 0.0f}};
    chain.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Chain ordering: CFO then DC offset vs DC offset then CFO", "[impairment_chain][ordering]") {
    ImpairmentChain chain1;
    chain1.add(std::make_unique<CfoImpairment>(0.0, 1e6));
    chain1.add(std::make_unique<DcOffsetImpairment>(0.5, 0.0));

    ImpairmentChain chain2;
    chain2.add(std::make_unique<DcOffsetImpairment>(0.5, 0.0));
    chain2.add(std::make_unique<CfoImpairment>(0.0, 1e6));

    std::vector<std::complex<float>> data1 = {{1.0f, 0.0f}};
    std::vector<std::complex<float>> data2 = {{1.0f, 0.0f}};

    chain1.apply(data1.data(), data1.size());
    chain2.apply(data2.data(), data2.size());

    REQUIRE_THAT(data1[0].real(), WithinAbs(1.5f, 1e-5f));
    REQUIRE_THAT(data2[0].real(), WithinAbs(1.5f, 1e-5f));
}

TEST_CASE("Chain with 3 impairments applies in sequence", "[impairment_chain][ordering]") {
    ImpairmentChain chain;
    chain.add(std::make_unique<DcOffsetImpairment>(1.0, 0.0));
    chain.add(std::make_unique<DcOffsetImpairment>(2.0, 0.0));
    chain.add(std::make_unique<DcOffsetImpairment>(3.0, 0.0));

    std::vector<std::complex<float>> data = {{0.0f, 0.0f}};
    chain.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(6.0f, 1e-5f));
}

TEST_CASE("Chain ordering with non-commutative impairments", "[impairment_chain][ordering]") {
    ImpairmentChain chain_a;
    chain_a.add(std::make_unique<DcOffsetImpairment>(1.0, 0.0));
    chain_a.add(std::make_unique<PhaseOffsetImpairment>(0.0));

    ImpairmentChain chain_b;
    chain_b.add(std::make_unique<PhaseOffsetImpairment>(0.0));
    chain_b.add(std::make_unique<DcOffsetImpairment>(1.0, 0.0));

    std::vector<std::complex<float>> data_a = {{0.0f, 1.0f}};
    std::vector<std::complex<float>> data_b = {{0.0f, 1.0f}};

    chain_a.apply(data_a.data(), data_a.size());
    chain_b.apply(data_b.data(), data_b.size());

    REQUIRE_THAT(data_a[0].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data_a[0].imag(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data_b[0].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data_b[0].imag(), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Chain names reflect insertion order", "[impairment_chain][ordering]") {
    ImpairmentChain chain;
    chain.add(std::make_unique<DcOffsetImpairment>());
    chain.add(std::make_unique<PhaseOffsetImpairment>());
    chain.add(std::make_unique<CfoImpairment>());

    REQUIRE(chain.at(0).name() == "dc_offset");
    REQUIRE(chain.at(1).name() == "phase_offset");
    REQUIRE(chain.at(2).name() == "cfo");
}
