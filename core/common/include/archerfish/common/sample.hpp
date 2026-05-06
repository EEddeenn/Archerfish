#pragma once

#include <cmath>
#include <complex>
#include <vector>

namespace archerfish::common {

enum class SampleFormat {
    CF32,
    CI16,
    CI8
};

struct SampleBuffer {
    std::vector<std::complex<float>> samples;
    double sample_rate{0.0};

    [[nodiscard]] size_t count() const { return samples.size(); }

    [[nodiscard]] double duration_sec() const {
        if (!std::isfinite(sample_rate) || sample_rate <= 0.0) return 0.0;
        return static_cast<double>(samples.size()) / sample_rate;
    }

    [[nodiscard]] double peak_amplitude() const {
        if (samples.empty()) return 0.0;
        double peak = 0.0;
        for (const auto& s : samples) {
            double mag = std::abs(s);
            if (!std::isfinite(mag)) continue;
            if (mag > peak) peak = mag;
        }
        return peak;
    }

    [[nodiscard]] double rms_amplitude() const {
        if (samples.empty()) return 0.0;
        double sum_sq = 0.0;
        size_t finite_count = 0;
        for (const auto& s : samples) {
            double mag = std::abs(s);
            if (!std::isfinite(mag)) continue;
            sum_sq += mag * mag;
            ++finite_count;
        }
        if (finite_count == 0) return 0.0;
        return std::sqrt(sum_sq / static_cast<double>(finite_count));
    }

    [[nodiscard]] double crest_factor() const {
        double rms = rms_amplitude();
        if (rms <= 0.0) return 0.0;
        return peak_amplitude() / rms;
    }
};

} // namespace archerfish::common
