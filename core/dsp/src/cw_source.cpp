#include "archerfish/dsp/cw_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace archerfish::dsp {

void CwSource::configure(const nlohmann::json& params) {
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("frequency_hz"))
        frequency_hz_ = params["frequency_hz"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
}

void CwSource::prepare() {
    samples_generated_ = 0;
}

size_t CwSource::render_block(std::complex<float>* out, size_t max_samples) {
    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_generated_ >= total_samples)
            return 0;
        total_available = total_samples - samples_generated_;
    }

    size_t to_generate = std::min(max_samples, total_available);

    const double phase_inc = 2.0 * M_PI * frequency_hz_ / sample_rate_;
    const float amp = static_cast<float>(amplitude_);

    for (size_t i = 0; i < to_generate; ++i) {
        double phase = phase_inc * static_cast<double>(samples_generated_ + i);
        float cos_p = static_cast<float>(std::cos(phase));
        float sin_p = static_cast<float>(std::sin(phase));
        out[i] = amp * std::complex<float>(cos_p, sin_p);
    }

    samples_generated_ += to_generate;
    return to_generate;
}

WaveformMetadata CwSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    meta.duration_sec = duration_sec_;
    meta.repeats = !duration_sec_.has_value();
    meta.nominal_bandwidth = 0.0;
    return meta;
}

void CwSource::reset() {
    samples_generated_ = 0;
}

} // namespace archerfish::dsp
