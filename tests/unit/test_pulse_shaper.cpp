#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "archerfish/dsp/pulse_shaper.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("RRC filter num_taps matches design parameters", "[dsp][pulse_shaper]") {
    RrcFilterDesign design;
    design.alpha = 0.35;
    design.span_symbols = 6;
    design.samples_per_symbol = 4;

    REQUIRE(design.num_taps() == 6 * 4 + 1);
}

TEST_CASE("RRC filter design returns correct number of taps", "[dsp][pulse_shaper]") {
    RrcFilterDesign design;
    design.alpha = 0.35;
    design.span_symbols = 6;
    design.samples_per_symbol = 8;

    auto taps = design.design();
    REQUIRE(taps.size() == design.num_taps());
}

TEST_CASE("RRC filter is symmetric", "[dsp][pulse_shaper]") {
    RrcFilterDesign design;
    design.alpha = 0.35;
    design.span_symbols = 6;
    design.samples_per_symbol = 4;

    auto taps = design.design();
    size_t n = taps.size();

    for (size_t i = 0; i < n / 2; ++i) {
        REQUIRE_THAT(taps[i], WithinAbs(taps[n - 1 - i], 1e-6f));
    }
}

TEST_CASE("RRC filter with different roll-off factors", "[dsp][pulse_shaper]") {
    for (double alpha : {0.1, 0.35, 0.5, 1.0}) {
        RrcFilterDesign design;
        design.alpha = alpha;
        design.span_symbols = 6;
        design.samples_per_symbol = 4;

        auto taps = design.design();
        REQUIRE(taps.size() == 25);

        double energy = 0.0;
        for (auto t : taps) energy += static_cast<double>(t) * static_cast<double>(t);
        REQUIRE(energy > 0.0);
    }
}

TEST_CASE("RRC filter taps are energy-normalized", "[dsp][pulse_shaper]") {
    RrcFilterDesign design;
    design.alpha = 0.35;
    design.span_symbols = 6;
    design.samples_per_symbol = 4;

    auto taps = design.design();
    double energy = 0.0;
    for (float t : taps) {
        energy += static_cast<double>(t) * static_cast<double>(t);
    }
    REQUIRE_THAT(energy, WithinAbs(1.0, 1e-4));
}

TEST_CASE("RRC filter rejects invalid design parameters", "[dsp][pulse_shaper]") {
    RrcFilterDesign design;

    design.alpha = 0.0;
    REQUIRE_THROWS_AS(design.design(), std::invalid_argument);

    design.alpha = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_THROWS_AS(design.design(), std::invalid_argument);

    design.alpha = 0.35;
    design.span_symbols = 0;
    REQUIRE_THROWS_AS(design.design(), std::invalid_argument);
    REQUIRE_THROWS_AS(design.num_taps(), std::invalid_argument);

    design.span_symbols = 6;
    design.samples_per_symbol = 0;
    REQUIRE_THROWS_AS(design.design(), std::invalid_argument);
    REQUIRE_THROWS_AS(design.num_taps(), std::invalid_argument);
}
