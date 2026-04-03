#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/common/rf_types.hpp"
#include "archerfish/scenario/scenario.hpp"

using namespace archerfish::common;
using namespace archerfish::scenario;
using Catch::Matchers::WithinAbs;

TEST_CASE("RfSettings default values", "[common][rf]") {
    RfSettings cfg;
    CHECK(cfg.freq_hz == 0.0);
    CHECK(cfg.rate_sps == 0.0);
    CHECK(cfg.gain_db == 0.0);
    CHECK_FALSE(cfg.bandwidth_hz.has_value());
    CHECK_FALSE(cfg.antenna.has_value());
}

TEST_CASE("RfSettings validation: freq must be positive", "[common][rf]") {
    RfSettings cfg;
    cfg.freq_hz = -1.0;
    cfg.rate_sps = 1e6;
    auto errors = cfg.validate();
    REQUIRE(errors.size() >= 1);
    CHECK(errors[0].code == "E_RF_INVALID_FREQ");
}

TEST_CASE("RfSettings validation: rate must be positive", "[common][rf]") {
    RfSettings cfg;
    cfg.freq_hz = 1e9;
    cfg.rate_sps = 0.0;
    auto errors = cfg.validate();
    REQUIRE(errors.size() >= 1);
    bool found = false;
    for (const auto& e : errors) {
        if (e.code == "E_RF_INVALID_RATE") found = true;
    }
    CHECK(found);
}

TEST_CASE("RfSettings validation: valid config produces no errors", "[common][rf]") {
    RfSettings cfg;
    cfg.freq_hz = 2.4e9;
    cfg.rate_sps = 20e6;
    cfg.gain_db = 20.0;
    auto errors = cfg.validate();
    CHECK(errors.empty());
}

TEST_CASE("RfSettings validation: gain out of range", "[common][rf]") {
    RfSettings cfg;
    cfg.freq_hz = 1e9;
    cfg.rate_sps = 1e6;
    cfg.gain_db = 200.0;
    auto errors = cfg.validate();
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].code == "E_RF_INVALID_GAIN");
}

TEST_CASE("RfSettings validation: bandwidth must be positive", "[common][rf]") {
    RfSettings cfg;
    cfg.freq_hz = 1e9;
    cfg.rate_sps = 1e6;
    cfg.bandwidth_hz = -5.0;
    auto errors = cfg.validate();
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].code == "E_RF_INVALID_BW");
}

TEST_CASE("TimeSpec default construction", "[common][rf]") {
    TimeSpec ts;
    CHECK(ts.seconds == 0);
    CHECK(ts.fractional_ns == 0.0);
}

TEST_CASE("TimeSpec to_seconds", "[common][rf]") {
    TimeSpec ts{1, 500000000.0};
    CHECK_THAT(ts.to_seconds(), WithinAbs(1.5, 1e-12));
}

TEST_CASE("TimeSpec from_seconds roundtrip", "[common][rf]") {
    double original = 123.456789;
    TimeSpec ts = TimeSpec::from_seconds(original);
    CHECK_THAT(ts.to_seconds(), WithinAbs(original, 1e-6));
}

TEST_CASE("TimeSpec from_seconds zero", "[common][rf]") {
    TimeSpec ts = TimeSpec::from_seconds(0.0);
    CHECK(ts.seconds == 0);
    CHECK_THAT(ts.fractional_ns, WithinAbs(0.0, 1e-6));
}

TEST_CASE("TimeSpec comparison operators", "[common][rf]") {
    TimeSpec a{1, 0.0};
    TimeSpec b{2, 0.0};
    TimeSpec c{1, 500000000.0};

    CHECK(a < b);
    CHECK(a < c);
    CHECK_FALSE(b < a);
    CHECK(b >= a);
    CHECK(c >= a);
    CHECK(a >= a);
    CHECK_FALSE(a < a);
}

TEST_CASE("Duration and DeviceId type aliases compile", "[common][rf]") {
    Duration d = 1.5;
    ChannelId ch = 0;
    DeviceId dev = "usrp0";
    CHECK(d == 1.5);
    CHECK(ch == 0);
    CHECK(dev == "usrp0");
}
