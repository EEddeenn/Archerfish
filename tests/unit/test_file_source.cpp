#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <fstream>
#include <vector>

#include "archerfish/dsp/file_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

static const char* fixture_path = TEST_FIXTURE_DIR "/test_cf32.bin";

TEST_CASE("Read CF32 binary file correctly", "[dsp][file]") {
    FileSource src;
    src.configure({{"path", fixture_path}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(20);
    size_t n = src.render_block(buf.data(), buf.size());

    REQUIRE(n == 10);
    REQUIRE_THAT(buf[0].real(), WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(buf[0].imag(), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(buf[1].real(), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(buf[1].imag(), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("EOF behavior returns 0 if not looping", "[dsp][file]") {
    FileSource src;
    src.configure({{"path", fixture_path}, {"sample_rate", 1e6}, {"loop", false}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);
}

TEST_CASE("Loop mode wraps around", "[dsp][file]") {
    FileSource src;
    src.configure({{"path", fixture_path}, {"sample_rate", 1e6}, {"loop", true}});
    src.prepare();

    std::vector<std::complex<float>> buf(25);
    size_t n = src.render_block(buf.data(), buf.size());

    REQUIRE(n == 25);

    REQUIRE_THAT(buf[10].real(), WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(buf[10].imag(), WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Finite duration stops at limit", "[dsp][file]") {
    FileSource src;
    src.configure({{"path", fixture_path}, {"sample_rate", 1e6}, {"loop", true}, {"duration_sec", 0.000005}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());

    REQUIRE(n == 5);
}

TEST_CASE("Missing file handled gracefully", "[dsp][file]") {
    FileSource src;
    src.configure({{"path", "/nonexistent/path/to/file.bin"}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(10);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 0);
}
