#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <complex>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include <archerfish/dsp/file_source.hpp>
#include <nlohmann/json.hpp>

using namespace archerfish::dsp;

TEST_CASE("detect_file_format recognizes .ci16 extension", "[ci16]") {
    REQUIRE(detect_file_format("test.ci16") == FileFormat::CI16);
    REQUIRE(detect_file_format("test.cf32") == FileFormat::CF32);
    REQUIRE(detect_file_format("test.dat") == FileFormat::CF32);
}

TEST_CASE("CI16 write and read round-trip", "[ci16]") {
    std::vector<std::complex<float>> samples = {
        {0.5f, 0.5f}, {-0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, -1.0f}
    };

    std::string path = "/tmp/archerfish_test_ci16.ci16";
    write_ci16(path, samples);

    REQUIRE(std::filesystem::exists(path));

    FileSource src;
    nlohmann::json params;
    params["path"] = path;
    params["sample_rate"] = 1e6;
    params["duration_sec"] = 1.0;
    src.configure(params);
    src.prepare();

    std::vector<std::complex<float>> buffer(samples.size());
    size_t n = src.render_block(buffer.data(), samples.size());
    REQUIRE(n == samples.size());

    for (size_t i = 0; i < samples.size(); ++i) {
        REQUIRE_THAT(buffer[i].real(), Catch::Matchers::WithinAbs(samples[i].real(), 0.001f));
        REQUIRE_THAT(buffer[i].imag(), Catch::Matchers::WithinAbs(samples[i].imag(), 0.001f));
    }

    std::filesystem::remove(path);
}

TEST_CASE("CI16 write sanitizes non-finite samples", "[ci16]") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    std::vector<std::complex<float>> samples = {
        {nan, inf},
        {-inf, 0.5f}
    };

    std::string path = "/tmp/archerfish_test_ci16_nonfinite.ci16";
    write_ci16(path, samples);

    FileSource src;
    nlohmann::json params;
    params["path"] = path;
    params["sample_rate"] = 1e6;
    params["duration_sec"] = 1.0;
    src.configure(params);
    src.prepare();

    std::vector<std::complex<float>> buffer(samples.size());
    size_t n = src.render_block(buffer.data(), samples.size());
    REQUIRE(n == samples.size());
    CHECK(buffer[0].real() == 0.0f);
    CHECK(buffer[0].imag() == 0.0f);
    CHECK(buffer[1].real() == 0.0f);
    REQUIRE_THAT(buffer[1].imag(), Catch::Matchers::WithinAbs(0.5f, 0.001f));

    std::filesystem::remove(path);
}

TEST_CASE("CI16 write reports output failures", "[ci16]") {
    CHECK_THROWS_AS(write_ci16("/tmp", {{1.0f, 0.0f}}), std::runtime_error);
}
