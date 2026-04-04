#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/dsp/ofdm_source.hpp"

#include <cmath>
#include <complex>
#include <nlohmann/json.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("OFDM source produces output", "[dsp][ofdm]") {
    archerfish::dsp::OfdmSource src;
    nlohmann::json params;
    params["sample_rate"] = 1e6;
    params["amplitude"] = 0.2;
    params["fft_size"] = 64;
    params["cyclic_prefix_size"] = 16;
    params["active_subcarriers"] = 60;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    std::vector<std::complex<float>> buf(4096);
    size_t n = src.render_block(buf.data(), 4096);
    REQUIRE(n > 0);
}

TEST_CASE("OFDM output respects duration", "[dsp][ofdm]") {
    archerfish::dsp::OfdmSource src;
    nlohmann::json params;
    params["sample_rate"] = 1e6;
    params["amplitude"] = 0.2;
    params["fft_size"] = 64;
    params["cyclic_prefix_size"] = 16;
    params["active_subcarriers"] = 60;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    size_t expected = static_cast<size_t>(0.001 * 1e6);
    std::vector<std::complex<float>> all;
    std::vector<std::complex<float>> buf(8192);
    while (true) {
        size_t n = src.render_block(buf.data(), 8192);
        if (n == 0) break;
        all.insert(all.end(), buf.begin(), buf.begin() + static_cast<ptrdiff_t>(n));
    }
    REQUIRE(all.size() == expected);
}

TEST_CASE("OFDM reset produces same output", "[dsp][ofdm]") {
    nlohmann::json params;
    params["sample_rate"] = 1e6;
    params["amplitude"] = 0.2;
    params["fft_size"] = 64;
    params["cyclic_prefix_size"] = 16;
    params["active_subcarriers"] = 60;
    params["duration_sec"] = 0.0005;
    params["seed"] = 42;

    std::vector<std::complex<float>> first_run;
    std::vector<std::complex<float>> second_run;

    {
        archerfish::dsp::OfdmSource src;
        src.configure(params);
        src.prepare();
        std::vector<std::complex<float>> buf(8192);
        while (true) {
            size_t n = src.render_block(buf.data(), 8192);
            if (n == 0) break;
            first_run.insert(first_run.end(), buf.begin(), buf.begin() + static_cast<ptrdiff_t>(n));
        }
    }

    {
        archerfish::dsp::OfdmSource src;
        src.configure(params);
        src.prepare();
        std::vector<std::complex<float>> buf(8192);
        while (true) {
            size_t n = src.render_block(buf.data(), 8192);
            if (n == 0) break;
            second_run.insert(second_run.end(), buf.begin(), buf.begin() + static_cast<ptrdiff_t>(n));
        }
    }

    REQUIRE(first_run.size() == second_run.size());
    for (size_t i = 0; i < first_run.size(); ++i) {
        REQUIRE(first_run[i] == second_run[i]);
    }
}

TEST_CASE("OFDM symbol size is fft_size + cp_size", "[dsp][ofdm]") {
    archerfish::dsp::OfdmSource src;
    nlohmann::json params;
    params["sample_rate"] = 1e6;
    params["amplitude"] = 0.2;
    params["fft_size"] = 128;
    params["cyclic_prefix_size"] = 32;
    params["active_subcarriers"] = 120;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    size_t symbol_samples = 128 + 32;
    size_t expected_total = static_cast<size_t>(0.001 * 1e6);
    std::vector<std::complex<float>> all;
    std::vector<std::complex<float>> buf(8192);
    while (true) {
        size_t n = src.render_block(buf.data(), 8192);
        if (n == 0) break;
        all.insert(all.end(), buf.begin(), buf.begin() + static_cast<ptrdiff_t>(n));
    }
    REQUIRE(all.size() == expected_total);
    CHECK(all.size() >= symbol_samples);
}

TEST_CASE("OFDM metadata reports correct bandwidth", "[dsp][ofdm]") {
    archerfish::dsp::OfdmSource src;
    nlohmann::json params;
    params["sample_rate"] = 2e6;
    params["amplitude"] = 0.3;
    params["fft_size"] = 64;
    params["cyclic_prefix_size"] = 16;
    params["active_subcarriers"] = 60;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    auto meta = src.report_metadata();
    REQUIRE_THAT(meta.nominal_bandwidth, WithinAbs(2e6, 1.0));
    REQUIRE_THAT(meta.peak_amplitude, WithinAbs(0.3, 0.01));
}

TEST_CASE("OFDM output is not all zeros", "[dsp][ofdm]") {
    archerfish::dsp::OfdmSource src;
    nlohmann::json params;
    params["sample_rate"] = 1e6;
    params["amplitude"] = 0.2;
    params["fft_size"] = 64;
    params["cyclic_prefix_size"] = 16;
    params["active_subcarriers"] = 60;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    std::vector<std::complex<float>> all;
    std::vector<std::complex<float>> buf(8192);
    while (true) {
        size_t n = src.render_block(buf.data(), 8192);
        if (n == 0) break;
        all.insert(all.end(), buf.begin(), buf.begin() + static_cast<ptrdiff_t>(n));
    }

    double energy = 0.0;
    for (auto& s : all) {
        energy += std::abs(s) * std::abs(s);
    }
    REQUIRE(energy > 0.0);
}
