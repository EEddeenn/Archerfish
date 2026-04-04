#include "archerfish/dsp/constellation.hpp"

#include <cmath>

#include "archerfish/common/constants.hpp"

namespace {

static uint32_t gray_decode(uint32_t x) {
    uint32_t mask = x;
    uint32_t result = x;
    for (int i = 0; i < 8; ++i) {
        mask >>= 1;
        result ^= mask;
    }
    return result;
}

} // namespace

namespace archerfish::dsp {

std::vector<std::complex<float>> build_constellation(ModulationType modulation) {
    std::vector<std::complex<float>> constellation;
    switch (modulation) {
    case ModulationType::BPSK:
        constellation = {{1.0f, 0.0f}, {-1.0f, 0.0f}};
        break;
    case ModulationType::QPSK: {
        float s = 1.0f / std::sqrt(2.0f);
        constellation = {
            {s, s},
            {-s, s},
            {s, -s},
            {-s, -s}
        };
        break;
    }
    case ModulationType::PSK8: {
        float a = static_cast<float>(archerfish::constants::kPi) / 8.0f;
        int angles[] = {1, 7, 15, 9, 3, 5, 13, 11};
        for (int k = 0; k < 8; ++k) {
            float theta = angles[k] * a;
            constellation.push_back({std::cos(theta), std::sin(theta)});
        }
        break;
    }
    case ModulationType::QAM16: {
        const float alpha = 1.0f / std::sqrt(10.0f);
        for (uint32_t sym = 0; sym < 16; ++sym) {
            uint32_t si = sym >> 2;
            uint32_t sq = sym & 3;
            si = gray_decode(si);
            sq = gray_decode(sq);
            float re = (2.0f * static_cast<float>(si) - 3.0f) * alpha;
            float im = (2.0f * static_cast<float>(sq) - 3.0f) * alpha;
            constellation.push_back({re, im});
        }
        break;
    }
    case ModulationType::QAM64: {
        const float alpha = 1.0f / std::sqrt(42.0f);
        for (uint32_t sym = 0; sym < 64; ++sym) {
            uint32_t si = sym >> 3;
            uint32_t sq = sym & 7;
            si = gray_decode(si);
            sq = gray_decode(sq);
            float re = (2.0f * static_cast<float>(si) - 7.0f) * alpha;
            float im = (2.0f * static_cast<float>(sq) - 7.0f) * alpha;
            constellation.push_back({re, im});
        }
        break;
    }
    case ModulationType::APSK16: {
        // DVB-S2 16-APSK: inner ring (4 points), outer ring (12 points)
        // Default ratio gamma = r2/r1 ≈ 2.85
        const double gamma = 2.85;
        const int n_inner = 4;
        const int n_outer = 12;
        // Compute normalization: average energy = 1.0
        // E_avg = (n_inner * r1^2 + n_outer * r2^2) / (n_inner + n_outer)
        // With r2 = gamma * r1: E_avg = r1^2 * (n_inner + n_outer * gamma^2) / (n_inner + n_outer)
        const double denom = static_cast<double>(n_inner + n_outer);
        const double coeff = static_cast<double>(n_inner) + static_cast<double>(n_outer) * gamma * gamma;
        double r1 = std::sqrt(denom / coeff);
        double r2 = gamma * r1;
        // Inner ring: 4 points equally spaced starting at pi/4 (to interleave with outer)
        for (int k = 0; k < n_inner; ++k) {
            double theta = archerfish::constants::kTwoPi * k / n_inner + archerfish::constants::kPi / 4.0;
            constellation.push_back({static_cast<float>(r1 * std::cos(theta)),
                                      static_cast<float>(r1 * std::sin(theta))});
        }
        // Outer ring: 12 points with pi/12 offset
        for (int k = 0; k < n_outer; ++k) {
            double theta = archerfish::constants::kTwoPi * k / n_outer + archerfish::constants::kPi / 12.0;
            constellation.push_back({static_cast<float>(r2 * std::cos(theta)),
                                      static_cast<float>(r2 * std::sin(theta))});
        }
        break;
    }
    case ModulationType::APSK32: {
        // DVB-S2 32-APSK: inner (4), middle (12), outer (16)
        // Default ratios: r2/r1 ≈ 2.72, r3/r1 ≈ 4.87
        const double gamma1 = 2.72;
        const double gamma2 = 4.87;
        const int n_inner = 4;
        const int n_middle = 12;
        const int n_outer = 16;
        const double denom = static_cast<double>(n_inner + n_middle + n_outer);
        const double coeff = static_cast<double>(n_inner)
                           + static_cast<double>(n_middle) * gamma1 * gamma1
                           + static_cast<double>(n_outer) * gamma2 * gamma2;
        double r1 = std::sqrt(denom / coeff);
        double r2 = gamma1 * r1;
        double r3 = gamma2 * r1;
        // Inner ring: 4 points, offset 0
        for (int k = 0; k < n_inner; ++k) {
            double theta = archerfish::constants::kTwoPi * k / n_inner;
            constellation.push_back({static_cast<float>(r1 * std::cos(theta)),
                                      static_cast<float>(r1 * std::sin(theta))});
        }
        // Middle ring: 12 points with pi/12 offset
        for (int k = 0; k < n_middle; ++k) {
            double theta = archerfish::constants::kTwoPi * k / n_middle + archerfish::constants::kPi / 12.0;
            constellation.push_back({static_cast<float>(r2 * std::cos(theta)),
                                      static_cast<float>(r2 * std::sin(theta))});
        }
        // Outer ring: 16 points, offset 0
        for (int k = 0; k < n_outer; ++k) {
            double theta = archerfish::constants::kTwoPi * k / n_outer;
            constellation.push_back({static_cast<float>(r3 * std::cos(theta)),
                                      static_cast<float>(r3 * std::sin(theta))});
        }
        break;
    }
    }
    return constellation;
}

} // namespace archerfish::dsp
