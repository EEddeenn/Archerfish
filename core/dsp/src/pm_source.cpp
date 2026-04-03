#include "archerfish/dsp/pm_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace archerfish::dsp {

void PmSource::configure(const nlohmann::json& params) {
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("carrier_freq_hz"))
        carrier_freq_hz_ = params["carrier_freq_hz"].get<double>();
    if (params.contains("mod_freq_hz"))
        mod_freq_hz_ = params["mod_freq_hz"].get<double>();
    if (params.contains("mod_index"))
        mod_index_ = params["mod_index"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
}

void PmSource::prepare() {
    carrier_phase_ = 0.0f;
    sample_index_ = 0;
}

size_t PmSource::render_block(std::complex<float>* out, size_t max_samples) {
    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (sample_index_ >= total_samples)
            return 0;
        total_available = total_samples - sample_index_;
    }

    size_t to_generate = std::min(max_samples, total_available);

    const float carrier_incr = static_cast<float>(2.0 * M_PI * carrier_freq_hz_ / sample_rate_);
    const float mod_incr = static_cast<float>(2.0 * M_PI * mod_freq_hz_ / sample_rate_);
    const float amp = static_cast<float>(amplitude_);
    const float idx = static_cast<float>(mod_index_);
    const float two_pi = static_cast<float>(2.0 * M_PI);

    for (size_t i = 0; i < to_generate; ++i) {
        float phase_modulation = idx * std::cos(mod_incr * static_cast<float>(sample_index_));
        float total_phase = carrier_phase_ + phase_modulation;

        out[i] = amp * std::complex<float>(std::cos(total_phase), std::sin(total_phase));

        carrier_phase_ += carrier_incr;
        if (carrier_phase_ > two_pi) carrier_phase_ -= two_pi;

        sample_index_++;
    }

    return to_generate;
}

WaveformMetadata PmSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    meta.duration_sec = duration_sec_;
    meta.repeats = !duration_sec_.has_value();
    // Carson's rule for PM: BW ≈ 2 * (β + 1) * fm
    meta.nominal_bandwidth = 2.0 * (mod_index_ + 1.0) * mod_freq_hz_;
    return meta;
}

void PmSource::reset() {
    carrier_phase_ = 0.0f;
    sample_index_ = 0;
}

} // namespace archerfish::dsp
