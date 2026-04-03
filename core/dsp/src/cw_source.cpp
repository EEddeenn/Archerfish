#include "archerfish/dsp/cw_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "archerfish/common/constants.hpp"

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
    phase_ = 0.0;
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

    const double phase_inc = archerfish::constants::kTwoPi * frequency_hz_ / sample_rate_;
    const float amp = static_cast<float>(amplitude_);
    const double two_pi = archerfish::constants::kTwoPi;

    for (size_t i = 0; i < to_generate; ++i) {
        out[i] = amp * std::complex<float>(static_cast<float>(std::cos(phase_)), static_cast<float>(std::sin(phase_)));
        phase_ += phase_inc;
        if (phase_ > two_pi) phase_ -= two_pi;
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
    phase_ = 0.0;
}

} // namespace archerfish::dsp
