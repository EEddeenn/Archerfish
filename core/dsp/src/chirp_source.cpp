#include "archerfish/dsp/chirp_source.hpp"

#include <algorithm>
#include <cmath>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void ChirpSource::configure(const nlohmann::json& params) {
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("f0_hz"))
        f0_hz_ = params["f0_hz"].get<double>();
    if (params.contains("f1_hz"))
        f1_hz_ = params["f1_hz"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
}

void ChirpSource::prepare() {
    samples_generated_ = 0;
}

size_t ChirpSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (!duration_sec_.has_value())
        return 0;

    size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
    if (samples_generated_ >= total_samples)
        return 0;

    size_t remaining = total_samples - samples_generated_;
    size_t to_generate = std::min(max_samples, remaining);

    double duration = duration_sec_.value();
    double freq_slope = (f1_hz_ - f0_hz_) / duration;
    float amp = static_cast<float>(amplitude_);
    double ts = 1.0 / sample_rate_;

    for (size_t i = 0; i < to_generate; ++i) {
        double t = static_cast<double>(samples_generated_ + i) * ts;
        double phase = archerfish::constants::kTwoPi * (f0_hz_ * t + freq_slope * t * t / 2.0);
        float cos_p = static_cast<float>(std::cos(phase));
        float sin_p = static_cast<float>(std::sin(phase));
        out[i] = amp * std::complex<float>(cos_p, sin_p);
    }

    samples_generated_ += to_generate;
    return to_generate;
}

WaveformMetadata ChirpSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    meta.duration_sec = duration_sec_;
    meta.repeats = false;
    double fmin = std::min(f0_hz_, f1_hz_);
    double fmax = std::max(f0_hz_, f1_hz_);
    meta.nominal_bandwidth = fmax - fmin;
    return meta;
}

void ChirpSource::reset() {
    samples_generated_ = 0;
}

} // namespace archerfish::dsp
