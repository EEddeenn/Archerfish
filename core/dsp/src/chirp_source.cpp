#include "archerfish/dsp/chirp_source.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void ChirpSource::configure(const nlohmann::json& params) {
    double next_f0_hz = f0_hz_;
    double next_f1_hz = f1_hz_;
    if (params.contains("f0_hz")) {
        next_f0_hz = number_param(params, "f0_hz");
        if (!std::isfinite(next_f0_hz)) {
            throw std::invalid_argument("f0_hz must be finite");
        }
    }
    if (params.contains("f1_hz")) {
        next_f1_hz = number_param(params, "f1_hz");
        if (!std::isfinite(next_f1_hz)) {
            throw std::invalid_argument("f1_hz must be finite");
        }
    }
    configure_common(params);
    f0_hz_ = next_f0_hz;
    f1_hz_ = next_f1_hz;
}

void ChirpSource::prepare() {
    reset_common();
}

size_t ChirpSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("ChirpSource render output buffer must not be null");
    }
    if (!duration_sec_.has_value())
        return 0;

    size_t total_samples = checked_sample_count(sample_rate_, duration_sec_.value());
    if (samples_produced_ >= total_samples)
        return 0;

    size_t remaining = total_samples - samples_produced_;
    size_t to_generate = std::min(max_samples, remaining);

    double duration = duration_sec_.value();
    double freq_slope = (f1_hz_ - f0_hz_) / duration;
    float amp = static_cast<float>(amplitude_);
    double ts = 1.0 / sample_rate_;

    for (size_t i = 0; i < to_generate; ++i) {
        double t = static_cast<double>(samples_produced_ + i) * ts;
        double phase = archerfish::constants::kTwoPi * (f0_hz_ * t + freq_slope * t * t / 2.0);
        float cos_p = static_cast<float>(std::cos(phase));
        float sin_p = static_cast<float>(std::sin(phase));
        out[i] = amp * std::complex<float>(cos_p, sin_p);
    }

    samples_produced_ += to_generate;
    return to_generate;
}

WaveformMetadata ChirpSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_;
    meta.crest_factor = amplitude_ > 0.0 ? 1.0 : 0.0;
    meta.repeats = false;
    double fmin = std::min(f0_hz_, f1_hz_);
    double fmax = std::max(f0_hz_, f1_hz_);
    meta.nominal_bandwidth = fmax - fmin;
    return meta;
}

void ChirpSource::reset() {
    reset_common();
}

} // namespace archerfish::dsp
