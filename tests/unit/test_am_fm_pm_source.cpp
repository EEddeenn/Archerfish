#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "archerfish/dsp/am_source.hpp"
#include "archerfish/dsp/fm_source.hpp"
#include "archerfish/dsp/pm_source.hpp"

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE("AM envelope modulates correctly", "[dsp][am]") {
    AmSource src;
    double amp = 0.2;
    double depth = 0.5;
    src.configure({{"amplitude", amp},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"mod_depth", depth},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(10000);
    src.render_block(buf.data(), buf.size());

    float max_mag = 0.0f;
    for (size_t i = 0; i < buf.size(); ++i) {
        float mag = std::abs(buf[i]);
        if (mag > max_mag) max_mag = mag;
    }

    float expected_peak = static_cast<float>(amp * (1.0 + depth));
    REQUIRE_THAT(max_mag, WithinAbs(expected_peak, 0.005f));
}

TEST_CASE("AM metadata is correct", "[dsp][am]") {
    AmSource src;
    src.configure({{"amplitude", 0.2},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"mod_depth", 0.5},
                   {"sample_rate", 1e6},
                   {"duration_sec", 0.001}});
    src.prepare();

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.3, 1e-9));
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(2000.0, 1e-9));
    REQUIRE(meta.duration_sec.has_value());
    REQUIRE_FALSE(meta.repeats);
}

TEST_CASE("AM phase accumulation across render_block calls", "[dsp][am]") {
    AmSource src;
    src.configure({{"amplitude", 1.0},
                   {"carrier_freq_hz", 100e3},
                   {"mod_freq_hz", 1000.0},
                   {"mod_depth", 0.5},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> all(100);
    src.render_block(all.data(), all.size());

    src.reset();

    std::vector<std::complex<float>> split(100);
    src.render_block(split.data(), 50);
    src.render_block(split.data() + 50, 50);

    for (size_t i = 0; i < 100; ++i) {
        REQUIRE_THAT(split[i].real(), WithinAbs(all[i].real(), 1e-6f));
        REQUIRE_THAT(split[i].imag(), WithinAbs(all[i].imag(), 1e-6f));
    }
}

TEST_CASE("FM constant amplitude", "[dsp][fm]") {
    FmSource src;
    src.configure({{"amplitude", 0.5},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(10000);
    src.render_block(buf.data(), buf.size());

    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE_THAT(std::abs(buf[i]), WithinAbs(0.5f, 0.005f));
    }
}

TEST_CASE("FM Carson's rule bandwidth in metadata", "[dsp][fm]") {
    FmSource src;
    src.configure({{"amplitude", 0.2},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    auto meta = src.report_metadata();
    double expected_bw = 2.0 * (5000.0 + 1000.0);
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(expected_bw, 1e-9));
}

TEST_CASE("FM phase accumulation across render_block calls", "[dsp][fm]") {
    FmSource src;
    src.configure({{"amplitude", 1.0},
                   {"carrier_freq_hz", 100e3},
                   {"mod_freq_hz", 1000.0},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> all(100);
    src.render_block(all.data(), all.size());

    src.reset();

    std::vector<std::complex<float>> split(100);
    src.render_block(split.data(), 50);
    src.render_block(split.data() + 50, 50);

    for (size_t i = 0; i < 100; ++i) {
        REQUIRE_THAT(split[i].real(), WithinAbs(all[i].real(), 1e-6f));
        REQUIRE_THAT(split[i].imag(), WithinAbs(all[i].imag(), 1e-6f));
    }
}

TEST_CASE("FM reset clears state", "[dsp][fm]") {
    FmSource src;
    src.configure({{"amplitude", 1.0},
                   {"carrier_freq_hz", 100e3},
                   {"mod_freq_hz", 1000.0},
                   {"deviation_hz", 5000.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf1(100);
    src.render_block(buf1.data(), buf1.size());

    src.reset();

    std::vector<std::complex<float>> buf2(100);
    src.render_block(buf2.data(), buf2.size());

    for (size_t i = 0; i < 100; ++i) {
        REQUIRE_THAT(buf2[i].real(), WithinAbs(buf1[i].real(), 1e-6f));
        REQUIRE_THAT(buf2[i].imag(), WithinAbs(buf1[i].imag(), 1e-6f));
    }
}

TEST_CASE("PM constant amplitude", "[dsp][pm]") {
    PmSource src;
    src.configure({{"amplitude", 0.5},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"mod_index", 1.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf(10000);
    src.render_block(buf.data(), buf.size());

    for (size_t i = 0; i < buf.size(); ++i) {
        REQUIRE_THAT(std::abs(buf[i]), WithinAbs(0.5f, 0.005f));
    }
}

TEST_CASE("PM Carson's rule bandwidth in metadata", "[dsp][pm]") {
    PmSource src;
    src.configure({{"amplitude", 0.2},
                   {"carrier_freq_hz", 0.0},
                   {"mod_freq_hz", 1000.0},
                   {"mod_index", 1.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    auto meta = src.report_metadata();
    double expected_bw = 2.0 * (1.0 + 1.0) * 1000.0;
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(expected_bw, 1e-9));
}

TEST_CASE("PM phase accumulation across render_block calls", "[dsp][pm]") {
    PmSource src;
    src.configure({{"amplitude", 1.0},
                   {"carrier_freq_hz", 100e3},
                   {"mod_freq_hz", 1000.0},
                   {"mod_index", 1.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> all(100);
    src.render_block(all.data(), all.size());

    src.reset();

    std::vector<std::complex<float>> split(100);
    src.render_block(split.data(), 50);
    src.render_block(split.data() + 50, 50);

    for (size_t i = 0; i < 100; ++i) {
        REQUIRE_THAT(split[i].real(), WithinAbs(all[i].real(), 1e-6f));
        REQUIRE_THAT(split[i].imag(), WithinAbs(all[i].imag(), 1e-6f));
    }
}

TEST_CASE("PM reset clears state", "[dsp][pm]") {
    PmSource src;
    src.configure({{"amplitude", 1.0},
                   {"carrier_freq_hz", 100e3},
                   {"mod_freq_hz", 1000.0},
                   {"mod_index", 1.0},
                   {"sample_rate", 1e6}});
    src.prepare();

    std::vector<std::complex<float>> buf1(100);
    src.render_block(buf1.data(), buf1.size());

    src.reset();

    std::vector<std::complex<float>> buf2(100);
    src.render_block(buf2.data(), buf2.size());

    for (size_t i = 0; i < 100; ++i) {
        REQUIRE_THAT(buf2[i].real(), WithinAbs(buf1[i].real(), 1e-6f));
        REQUIRE_THAT(buf2[i].imag(), WithinAbs(buf1[i].imag(), 1e-6f));
    }
}
