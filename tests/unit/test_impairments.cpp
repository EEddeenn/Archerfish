#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/impairments/awgn.hpp"
#include "archerfish/impairments/cfo.hpp"
#include "archerfish/impairments/phase_offset.hpp"
#include "archerfish/impairments/iq_imbalance.hpp"
#include "archerfish/impairments/dc_offset.hpp"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("AWGN adds noise", "[impairments][awgn]") {
    archerfish::impairments::AwgnImpairment awgn(0.1, 42);
    std::vector<std::complex<float>> data(1000, {1.0f, 0.0f});
    auto original = data;

    awgn.apply(data.data(), data.size());

    bool changed = false;
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] != original[i]) {
            changed = true;
            break;
        }
    }
    REQUIRE(changed);
}

TEST_CASE("AWGN noise has zero mean", "[impairments][awgn]") {
    archerfish::impairments::AwgnImpairment awgn(1.0, 12345);
    std::vector<std::complex<float>> data(100000, {0.0f, 0.0f});

    awgn.apply(data.data(), data.size());

    double mean_i = 0.0, mean_q = 0.0;
    for (const auto& s : data) {
        mean_i += static_cast<double>(s.real());
        mean_q += static_cast<double>(s.imag());
    }
    mean_i /= static_cast<double>(data.size());
    mean_q /= static_cast<double>(data.size());

    REQUIRE_THAT(mean_i, WithinAbs(0.0, 0.01));
    REQUIRE_THAT(mean_q, WithinAbs(0.0, 0.01));
}

TEST_CASE("AWGN name", "[impairments][awgn]") {
    archerfish::impairments::AwgnImpairment awgn;
    REQUIRE(awgn.name() == "awgn");
    REQUIRE_FALSE(awgn.name().empty());
}

TEST_CASE("AWGN disable", "[impairments][awgn]") {
    archerfish::impairments::AwgnImpairment awgn(1.0, 42);
    std::vector<std::complex<float>> data(100, {1.0f, 0.0f});
    auto original = data;

    awgn.set_enabled(false);
    REQUIRE_FALSE(awgn.enabled());
    awgn.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(data[i] == original[i]);
    }
}

TEST_CASE("CFO zero offset preserves data", "[impairments][cfo]") {
    archerfish::impairments::CfoImpairment cfo(0.0, 1e6);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};
    auto original = data;

    cfo.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(original[i].real(), 1e-5f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(original[i].imag(), 1e-5f));
    }
}

TEST_CASE("CFO applies phase rotation", "[impairments][cfo]") {
    double sample_rate = 1e6;
    double cfo_hz = sample_rate / 4.0;
    archerfish::impairments::CfoImpairment cfo(cfo_hz, sample_rate);

    std::vector<std::complex<float>> data = {{1.0f, 0.0f}, {1.0f, 0.0f}};
    cfo.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[1].real(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[1].imag(), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("CFO sample counter persists", "[impairments][cfo]") {
    double sample_rate = 4.0;
    archerfish::impairments::CfoImpairment cfo(1.0, sample_rate);

    std::vector<std::complex<float>> data1 = {{1.0f, 0.0f}, {1.0f, 0.0f}};
    cfo.apply(data1.data(), data1.size());

    std::vector<std::complex<float>> data2 = {{1.0f, 0.0f}};
    cfo.apply(data2.data(), data2.size());

    REQUIRE_THAT(data2[0].real(), WithinAbs(-1.0f, 1e-5f));
    REQUIRE_THAT(data2[0].imag(), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("CFO name", "[impairments][cfo]") {
    archerfish::impairments::CfoImpairment cfo;
    REQUIRE(cfo.name() == "cfo");
    REQUIRE_FALSE(cfo.name().empty());
}

TEST_CASE("CFO disable", "[impairments][cfo]") {
    archerfish::impairments::CfoImpairment cfo(1000.0, 1e6);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};
    auto original = data;

    cfo.set_enabled(false);
    REQUIRE_FALSE(cfo.enabled());
    cfo.apply(data.data(), data.size());

    REQUIRE(data[0] == original[0]);
}

TEST_CASE("PhaseOffset zero preserves data", "[impairments][phase_offset]") {
    archerfish::impairments::PhaseOffsetImpairment po(0.0);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}, {0.0f, 1.0f}};
    auto original = data;

    po.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(original[i].real(), 1e-5f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(original[i].imag(), 1e-5f));
    }
}

TEST_CASE("PhaseOffset pi/2 rotates correctly", "[impairments][phase_offset]") {
    archerfish::impairments::PhaseOffsetImpairment po(M_PI / 2.0);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};

    po.apply(data.data(), data.size());

    REQUIRE_THAT(data[0].real(), WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("PhaseOffset name", "[impairments][phase_offset]") {
    archerfish::impairments::PhaseOffsetImpairment po;
    REQUIRE(po.name() == "phase_offset");
    REQUIRE_FALSE(po.name().empty());
}

TEST_CASE("PhaseOffset disable", "[impairments][phase_offset]") {
    archerfish::impairments::PhaseOffsetImpairment po(M_PI / 4.0);
    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};
    auto original = data;

    po.set_enabled(false);
    REQUIRE_FALSE(po.enabled());
    po.apply(data.data(), data.size());

    REQUIRE(data[0] == original[0]);
}

TEST_CASE("IQImbalance zero imbalance preserves data", "[impairments][iq_imbalance]") {
    archerfish::impairments::IqImbalanceImpairment iq(0.0, 0.0);
    std::vector<std::complex<float>> data = {{1.0f, 0.5f}, {0.3f, 0.7f}};
    auto original = data;

    iq.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(original[i].real(), 1e-5f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(original[i].imag(), 1e-5f));
    }
}

TEST_CASE("IQImbalance gain scales I", "[impairments][iq_imbalance]") {
    double gain_db = 6.0206;
    archerfish::impairments::IqImbalanceImpairment iq(gain_db, 0.0);
    std::vector<std::complex<float>> data = {{1.0f, 1.0f}};

    iq.apply(data.data(), data.size());

    double a = std::pow(10.0, gain_db / 20.0);
    double g = (a - 1.0) / (a + 1.0);
    float expected_i = static_cast<float>(1.0 + g) * 1.0f;
    float expected_q = static_cast<float>(1.0 - g) * 1.0f;

    REQUIRE_THAT(data[0].real(), WithinAbs(expected_i, 1e-4f));
    REQUIRE_THAT(data[0].imag(), WithinAbs(expected_q, 1e-4f));
}

TEST_CASE("IQImbalance name", "[impairments][iq_imbalance]") {
    archerfish::impairments::IqImbalanceImpairment iq;
    REQUIRE(iq.name() == "iq_imbalance");
    REQUIRE_FALSE(iq.name().empty());
}

TEST_CASE("IQImbalance disable", "[impairments][iq_imbalance]") {
    archerfish::impairments::IqImbalanceImpairment iq(6.0, 0.1);
    std::vector<std::complex<float>> data = {{1.0f, 1.0f}};
    auto original = data;

    iq.set_enabled(false);
    REQUIRE_FALSE(iq.enabled());
    iq.apply(data.data(), data.size());

    REQUIRE(data[0] == original[0]);
}

TEST_CASE("DCOffset adds constant", "[impairments][dc_offset]") {
    archerfish::impairments::DcOffsetImpairment dc(0.1, 0.2);
    std::vector<std::complex<float>> data(10, {1.0f, 1.0f});

    dc.apply(data.data(), data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE_THAT(data[i].real(), WithinAbs(1.1f, 1e-5f));
        REQUIRE_THAT(data[i].imag(), WithinAbs(1.2f, 1e-5f));
    }
}

TEST_CASE("DCOffset name", "[impairments][dc_offset]") {
    archerfish::impairments::DcOffsetImpairment dc;
    REQUIRE(dc.name() == "dc_offset");
    REQUIRE_FALSE(dc.name().empty());
}

TEST_CASE("DCOffset disable", "[impairments][dc_offset]") {
    archerfish::impairments::DcOffsetImpairment dc(0.1, 0.2);
    std::vector<std::complex<float>> data = {{1.0f, 1.0f}};
    auto original = data;

    dc.set_enabled(false);
    REQUIRE_FALSE(dc.enabled());
    dc.apply(data.data(), data.size());

    REQUIRE(data[0] == original[0]);
}
