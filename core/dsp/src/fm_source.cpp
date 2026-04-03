#include "archerfish/dsp/fm_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace archerfish::dsp {

void FmSource::configure(const nlohmann::json& params) {
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("carrier_freq_hz"))
        carrier_freq_hz_ = params["carrier_freq_hz"].get<double>();
    if (params.contains("mod_freq_hz"))
        mod_freq_hz_ = params["mod_freq_hz"].get<double>();
    if (params.contains("deviation_hz"))
        deviation_hz_ = params["deviation_hz"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
}

void FmSource::prepare() {
    phase_ = 0.0;
    sample_index_ = 0;
}

size_t FmSource::render_block(std::complex<float>* out, size_t max_samples) {
    size_t to_generate = max_samples;
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (sample_index_ >= total_samples)
            return 0;
        to_generate = std::min(max_samples, total_samples - sample_index_);
    }

    const double carrier_incr = 2.0 * M_PI * carrier_freq_hz_ / sample_rate_;
    const double mod_incr = 2.0 * M_PI * mod_freq_hz_ / sample_rate_;
    const double sensitivity = 2.0 * M_PI * deviation_hz_ / sample_rate_;
    const float amp = static_cast<float>(amplitude_);

    for (size_t i = 0; i < to_generate; ++i) {
        double mod_signal = std::cos(mod_incr * static_cast<double>(sample_index_));
        phase_ += carrier_incr + sensitivity * mod_signal;

        // Wrap phase to [-π, π) — GNU Radio pattern
        phase_ = std::fmod(phase_ + M_PI, 2.0 * M_PI) - M_PI;

        out[i] = amp * std::complex<float>(static_cast<float>(std::cos(phase_)),
                                            static_cast<float>(std::sin(phase_)));

        sample_index_++;
    }

    return to_generate;
}

WaveformMetadata FmSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    meta.duration_sec = duration_sec_;
    meta.repeats = !duration_sec_.has_value();
    // Carson's rule: BW = 2 * (deviation + mod_freq)
    meta.nominal_bandwidth = 2.0 * (deviation_hz_ + mod_freq_hz_);
    return meta;
}

void FmSource::reset() {
    phase_ = 0.0;
    sample_index_ = 0;
}

} // namespace archerfish::dsp
