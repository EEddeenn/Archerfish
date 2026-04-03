#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/common/sample.hpp"

using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

TEST_CASE("SampleBuffer default construction", "[common][sample]") {
    SampleBuffer buf;
    CHECK(buf.samples.empty());
    CHECK(buf.sample_rate == 0.0);
    CHECK(buf.count() == 0);
}

TEST_CASE("SampleBuffer count", "[common][sample]") {
    SampleBuffer buf;
    buf.samples = {{1.0f, 0.0f}, {0.0f, 1.0f}, {0.5f, 0.5f}};
    CHECK(buf.count() == 3);
}

TEST_CASE("SampleBuffer duration_sec", "[common][sample]") {
    SampleBuffer buf;
    buf.sample_rate = 1e6;
    buf.samples.resize(1000);
    CHECK_THAT(buf.duration_sec(), WithinAbs(0.001, 1e-9));
}

TEST_CASE("SampleBuffer duration_sec with zero rate", "[common][sample]") {
    SampleBuffer buf;
    buf.sample_rate = 0.0;
    buf.samples.resize(100);
    CHECK(buf.duration_sec() == 0.0);
}

TEST_CASE("SampleBuffer peak_amplitude", "[common][sample]") {
    SampleBuffer buf;
    buf.samples = {{0.5f, 0.0f}, {0.0f, 0.8f}, {0.3f, 0.3f}};
    float expected_peak = 0.8f;
    CHECK_THAT(buf.peak_amplitude(), WithinAbs(expected_peak, 1e-6));
}

TEST_CASE("SampleBuffer rms_amplitude on known DC signal", "[common][sample]") {
    SampleBuffer buf;
    buf.samples = {{1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}};
    CHECK_THAT(buf.rms_amplitude(), WithinAbs(1.0, 1e-6));
}

TEST_CASE("SampleBuffer crest_factor on constant signal", "[common][sample]") {
    SampleBuffer buf;
    buf.samples = {{1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}};
    CHECK_THAT(buf.crest_factor(), WithinAbs(1.0, 1e-6));
}

TEST_CASE("SampleBuffer empty edge cases", "[common][sample]") {
    SampleBuffer buf;
    CHECK(buf.peak_amplitude() == 0.0);
    CHECK(buf.rms_amplitude() == 0.0);
    CHECK(buf.crest_factor() == 0.0);
    CHECK(buf.duration_sec() == 0.0);
}

TEST_CASE("SampleBuffer crest_factor on impulse", "[common][sample]") {
    SampleBuffer buf;
    buf.samples = {{10.0f, 0.0f}, {0.1f, 0.0f}, {0.1f, 0.0f}, {0.1f, 0.0f}};
    double peak = buf.peak_amplitude();
    double rms = buf.rms_amplitude();
    CHECK_THAT(buf.crest_factor(), WithinAbs(peak / rms, 1e-6));
    CHECK(buf.crest_factor() > 1.0);
}
