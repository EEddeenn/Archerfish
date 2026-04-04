#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/dsp/modulator.hpp"

#include <cmath>
#include <complex>
#include <nlohmann/json.hpp>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

double constellation_avg_energy(const std::vector<std::complex<float>>& c) {
    double sum = 0.0;
    for (auto& p : c) {
        sum += std::abs(p) * std::abs(p);
    }
    return sum / static_cast<double>(c.size());
}

} // namespace

TEST_CASE("APSK16 constellation has 16 points", "[dsp][apsk]") {
    archerfish::dsp::ModulatorSource src;
    nlohmann::json params;
    params["modulation"] = "apsk16";
    params["symbol_rate"] = 1e6;
    params["samples_per_symbol"] = 4;
    params["rrc_alpha"] = 0.35;
    params["amplitude"] = 0.2;
    params["sample_rate"] = 4e6;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    auto& c = src.constellation();
    REQUIRE(c.size() == 16);
}

TEST_CASE("APSK16 average energy is normalized to 1.0", "[dsp][apsk]") {
    archerfish::dsp::ModulatorSource src;
    nlohmann::json params;
    params["modulation"] = "apsk16";
    params["symbol_rate"] = 1e6;
    params["samples_per_symbol"] = 4;
    params["rrc_alpha"] = 0.35;
    params["amplitude"] = 0.2;
    params["sample_rate"] = 4e6;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    auto& c = src.constellation();
    REQUIRE(c.size() == 16);
    double energy = constellation_avg_energy(c);
    REQUIRE_THAT(energy, WithinAbs(1.0, 0.01));
}

TEST_CASE("APSK16 has two distinct ring radii", "[dsp][apsk]") {
    archerfish::dsp::ModulatorSource src;
    nlohmann::json params;
    params["modulation"] = "apsk16";
    params["symbol_rate"] = 1e6;
    params["samples_per_symbol"] = 4;
    params["rrc_alpha"] = 0.35;
    params["amplitude"] = 0.2;
    params["sample_rate"] = 4e6;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    auto& c = src.constellation();
    std::vector<double> mags;
    for (auto& p : c) {
        mags.push_back(std::abs(p));
    }
    std::sort(mags.begin(), mags.end());

    int inner_count = 0;
    int outer_count = 0;
    double inner_r = mags[0];
    double outer_r = mags[15];
    for (auto m : mags) {
        if (std::abs(m - inner_r) < 0.01) inner_count++;
        else if (std::abs(m - outer_r) < 0.01) outer_count++;
    }
    REQUIRE(inner_count == 4);
    REQUIRE(outer_count == 12);

    double ratio = outer_r / inner_r;
    REQUIRE_THAT(ratio, WithinAbs(2.85, 0.1));
}

TEST_CASE("APSK32 constellation has 32 points", "[dsp][apsk]") {
    archerfish::dsp::ModulatorSource src;
    nlohmann::json params;
    params["modulation"] = "apsk32";
    params["symbol_rate"] = 1e6;
    params["samples_per_symbol"] = 4;
    params["rrc_alpha"] = 0.35;
    params["amplitude"] = 0.2;
    params["sample_rate"] = 4e6;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    auto& c = src.constellation();
    REQUIRE(c.size() == 32);
}

TEST_CASE("APSK32 average energy is normalized to 1.0", "[dsp][apsk]") {
    archerfish::dsp::ModulatorSource src;
    nlohmann::json params;
    params["modulation"] = "apsk32";
    params["symbol_rate"] = 1e6;
    params["samples_per_symbol"] = 4;
    params["rrc_alpha"] = 0.35;
    params["amplitude"] = 0.2;
    params["sample_rate"] = 4e6;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    auto& c = src.constellation();
    REQUIRE(c.size() == 32);
    double energy = constellation_avg_energy(c);
    REQUIRE_THAT(energy, WithinAbs(1.0, 0.01));
}

TEST_CASE("APSK32 has three distinct ring radii", "[dsp][apsk]") {
    archerfish::dsp::ModulatorSource src;
    nlohmann::json params;
    params["modulation"] = "apsk32";
    params["symbol_rate"] = 1e6;
    params["samples_per_symbol"] = 4;
    params["rrc_alpha"] = 0.35;
    params["amplitude"] = 0.2;
    params["sample_rate"] = 4e6;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();

    auto& c = src.constellation();
    std::vector<double> mags;
    for (auto& p : c) {
        mags.push_back(std::abs(p));
    }
    std::sort(mags.begin(), mags.end());

    double r1 = mags[0];
    double r3 = mags[31];

    int n_inner = 0, n_mid = 0, n_outer = 0;
    for (auto m : mags) {
        if (std::abs(m - r1) < 0.02) n_inner++;
        else if (std::abs(m - r3) < 0.02) n_outer++;
        else n_mid++;
    }
    REQUIRE(n_inner == 4);
    REQUIRE(n_mid == 12);
    REQUIRE(n_outer == 16);
}

TEST_CASE("APSK16 and APSK32 produce non-zero output", "[dsp][apsk]") {
    for (const char* mod : {"apsk16", "apsk32"}) {
        archerfish::dsp::ModulatorSource src;
        nlohmann::json params;
        params["modulation"] = mod;
        params["symbol_rate"] = 1e6;
        params["samples_per_symbol"] = 4;
        params["rrc_alpha"] = 0.35;
        params["amplitude"] = 0.2;
        params["sample_rate"] = 4e6;
        params["duration_sec"] = 0.001;
        params["seed"] = 42;
        src.configure(params);
        src.prepare();

        std::vector<std::complex<float>> buf(4096);
        size_t n = src.render_block(buf.data(), 4096);
        REQUIRE(n > 0);

        double peak = 0.0;
        for (size_t i = 0; i < n; ++i) {
            peak = std::max(peak, static_cast<double>(std::abs(buf[i])));
        }
        REQUIRE(peak > 0.0);
    }
}

TEST_CASE("APSK16 alias 16apsk is accepted", "[dsp][apsk]") {
    archerfish::dsp::ModulatorSource src;
    nlohmann::json params;
    params["modulation"] = "16apsk";
    params["symbol_rate"] = 1e6;
    params["samples_per_symbol"] = 4;
    params["rrc_alpha"] = 0.35;
    params["amplitude"] = 0.2;
    params["sample_rate"] = 4e6;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();
    REQUIRE(src.constellation().size() == 16);
}

TEST_CASE("APSK32 alias 32apsk is accepted", "[dsp][apsk]") {
    archerfish::dsp::ModulatorSource src;
    nlohmann::json params;
    params["modulation"] = "32apsk";
    params["symbol_rate"] = 1e6;
    params["samples_per_symbol"] = 4;
    params["rrc_alpha"] = 0.35;
    params["amplitude"] = 0.2;
    params["sample_rate"] = 4e6;
    params["duration_sec"] = 0.001;
    params["seed"] = 42;
    src.configure(params);
    src.prepare();
    REQUIRE(src.constellation().size() == 32);
}
