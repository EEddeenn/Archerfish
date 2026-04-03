#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <archerfish/cli/cmd_wave.hpp>

#include <complex>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace archerfish::cli;
using Catch::Matchers::WithinAbs;

static std::string make_temp_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

TEST_CASE("wave gen cw produces file with correct sample count", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_cw.cf32");
    double rate = 1e6;
    double duration = 0.01;

    int rc = cmd_wave_gen_cw(opts, rate, duration, 0.5, out);
    REQUIRE(rc == 0);

    size_t expected_samples = static_cast<size_t>(rate * duration);
    size_t file_size = std::filesystem::file_size(out);
    size_t actual_samples = file_size / sizeof(std::complex<float>);
    REQUIRE(actual_samples == expected_samples);

    std::filesystem::remove(out);
}

TEST_CASE("wave gen chirp produces file", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_chirp.cf32");

    int rc = cmd_wave_gen_chirp(opts, 1e6, 0.005, 100e3, 500e3, 0.3, out);
    REQUIRE(rc == 0);
    REQUIRE(std::filesystem::exists(out));
    REQUIRE(std::filesystem::file_size(out) > 0);

    std::filesystem::remove(out);
}

TEST_CASE("wave gen qpsk produces file", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_qpsk.cf32");

    int rc = cmd_wave_gen_qpsk(opts, 1e6, 4, 0.35, 0.001, 0.5, out);
    REQUIRE(rc == 0);
    REQUIRE(std::filesystem::exists(out));
    REQUIRE(std::filesystem::file_size(out) > 0);

    std::filesystem::remove(out);
}

TEST_CASE("wave inspect reports correct metadata for generated file", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_inspect.cf32");
    double rate = 1e6;
    double duration = 0.01;
    double amplitude = 0.5;

    cmd_wave_gen_cw(opts, rate, duration, amplitude, out);

    opts.json_output = true;
    int rc = cmd_wave_inspect(opts, out);
    REQUIRE(rc == 0);

    size_t file_size = std::filesystem::file_size(out);
    size_t actual_samples = file_size / sizeof(std::complex<float>);
    REQUIRE(actual_samples == static_cast<size_t>(rate * duration));

    std::filesystem::remove(out);
}
