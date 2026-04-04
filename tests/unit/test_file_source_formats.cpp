#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <fstream>
#include <string>
#include <vector>

#include "archerfish/dsp/file_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

static const char* kTempDir = "/tmp/archerfish_test";

static std::string write_cf32_file(const std::string& name, const std::vector<std::complex<float>>& data) {
    std::string path = std::string(kTempDir) + "/" + name;
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(std::complex<float>));
    ofs.close();
    return path;
}

static void cleanup(const std::string& path) {
    std::remove(path.c_str());
}

TEST_CASE("File source reads CF32 roundtrip", "[dsp][file_source][formats]") {
    std::vector<std::complex<float>> original(100);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = {static_cast<float>(i) * 0.01f, static_cast<float>(i) * -0.01f};
    }

    std::string path = write_cf32_file("roundtrip.cf32", original);

    FileSource src;
    src.configure({{"path", path}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(100);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 100);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf[i].real(), WithinAbs(original[i].real(), 1e-6f));
        REQUIRE_THAT(buf[i].imag(), WithinAbs(original[i].imag(), 1e-6f));
    }

    cleanup(path);
}

TEST_CASE("File source loops when configured", "[dsp][file_source][formats]") {
    std::vector<std::complex<float>> data(10);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = {static_cast<float>(i), 0.0f};
    }

    std::string path = write_cf32_file("loop.cf32", data);

    FileSource src;
    src.configure({{"path", path}, {"sample_rate", 1e6}, {"loop", true}});
    src.prepare();

    std::vector<std::complex<float>> buf(25);
    size_t n = src.render_block(buf.data(), buf.size());
    REQUIRE(n == 25);

    REQUIRE_THAT(buf[0].real(), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(buf[10].real(), WithinAbs(0.0f, 1e-6f));

    cleanup(path);
}

TEST_CASE("File source returns zero after EOF without loop", "[dsp][file_source][formats]") {
    std::vector<std::complex<float>> data(10, {1.0f, 0.0f});
    std::string path = write_cf32_file("eof.cf32", data);

    FileSource src;
    src.configure({{"path", path}, {"sample_rate", 1e6}, {"loop", false}});
    src.prepare();

    std::vector<std::complex<float>> buf(10);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 10);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 0);

    cleanup(path);
}

TEST_CASE("File source reset restarts from beginning", "[dsp][file_source][formats]") {
    std::vector<std::complex<float>> data(10);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = {static_cast<float>(i), 0.0f};
    }

    std::string path = write_cf32_file("reset.cf32", data);

    FileSource src;
    src.configure({{"path", path}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf1(10);
    src.render_block(buf1.data(), buf1.size());

    src.reset();

    std::vector<std::complex<float>> buf2(10);
    size_t n = src.render_block(buf2.data(), buf2.size());
    REQUIRE(n == 10);

    for (size_t i = 0; i < n; ++i) {
        REQUIRE_THAT(buf2[i].real(), WithinAbs(buf1[i].real(), 1e-6f));
    }

    cleanup(path);
}

TEST_CASE("File source with large file", "[dsp][file_source][formats]") {
    const size_t N = 100000;
    std::vector<std::complex<float>> data(N, {0.5f, -0.5f});
    std::string path = write_cf32_file("large.cf32", data);

    FileSource src;
    src.configure({{"path", path}, {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(50000);
    size_t n1 = src.render_block(buf.data(), buf.size());
    REQUIRE(n1 == 50000);

    size_t n2 = src.render_block(buf.data(), buf.size());
    REQUIRE(n2 == 50000);

    cleanup(path);
}
