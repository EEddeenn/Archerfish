#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <complex>
#include <memory>
#include <vector>

#include "archerfish/impairments/cfo.hpp"
#include "archerfish/impairments/awgn.hpp"
#include "archerfish/impairments/phase_offset.hpp"
#include "archerfish/impairments/iq_imbalance.hpp"
#include "archerfish/impairments/dc_offset.hpp"
#include "archerfish/impairments/impairment_chain.hpp"

using Catch::Matchers::WithinAbs;
using namespace archerfish::impairments;

TEST_CASE("CFO toggle enable/disable preserves data when disabled", "[impairments][enable_disable]") {
    CfoImpairment cfo(1000.0, 1e6);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};
    auto original = data;

    cfo.set_enabled(false);
    cfo.apply(data.data(), data.size());
    REQUIRE(data[0] == original[0]);

    cfo.set_enabled(true);
    REQUIRE(cfo.enabled());
}

TEST_CASE("AWGN toggle preserves data when disabled", "[impairments][enable_disable]") {
    AwgnImpairment awgn(1.0, 42);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};
    auto original = data;

    awgn.set_enabled(false);
    awgn.apply(data.data(), data.size());
    REQUIRE(data[0] == original[0]);

    awgn.set_enabled(true);
    REQUIRE(awgn.enabled());
}

TEST_CASE("PhaseOffset toggle preserves data when disabled", "[impairments][enable_disable]") {
    PhaseOffsetImpairment po(1.0);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};
    auto original = data;

    po.set_enabled(false);
    po.apply(data.data(), data.size());
    REQUIRE(data[0] == original[0]);

    po.set_enabled(true);
    REQUIRE(po.enabled());
}

TEST_CASE("IQImbalance toggle preserves data when disabled", "[impairments][enable_disable]") {
    IqImbalanceImpairment iq(6.0, 0.5);
    std::vector<std::complex<float>> data = {{1.0f, 1.0f}};
    auto original = data;

    iq.set_enabled(false);
    iq.apply(data.data(), data.size());
    REQUIRE(data[0] == original[0]);

    iq.set_enabled(true);
    REQUIRE(iq.enabled());
}

TEST_CASE("DCOffset toggle preserves data when disabled", "[impairments][enable_disable]") {
    DcOffsetImpairment dc(0.5, -0.3);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};
    auto original = data;

    dc.set_enabled(false);
    dc.apply(data.data(), data.size());
    REQUIRE(data[0] == original[0]);

    dc.set_enabled(true);
    REQUIRE(dc.enabled());
}

TEST_CASE("Chain disable middle element skips only that element", "[impairments][enable_disable]") {
    ImpairmentChain chain;
    chain.add(std::make_unique<DcOffsetImpairment>(1.0, 0.0));
    chain.add(std::make_unique<DcOffsetImpairment>(2.0, 0.0));
    chain.add(std::make_unique<DcOffsetImpairment>(4.0, 0.0));

    chain.set_enabled(1, false);

    std::vector<std::complex<float>> data = {{0.0f, 0.0f}};
    chain.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(5.0f, 1e-5f));
}
