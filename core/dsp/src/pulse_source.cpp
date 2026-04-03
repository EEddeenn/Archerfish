#include "archerfish/dsp/pulse_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace archerfish::dsp {

void PulseSource::configure(const nlohmann::json& params) {
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("frequency_hz"))
        frequency_hz_ = params["frequency_hz"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("pulse_width_sec"))
        pulse_width_sec_ = params["pulse_width_sec"].get<double>();
    if (params.contains("pri_sec"))
        pri_sec_ = params["pri_sec"].get<double>();
    if (params.contains("mode"))
        mode_ = params["mode"].get<std::string>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
}

void PulseSource::prepare() {
    pw_samples_ = static_cast<size_t>(std::round(pulse_width_sec_ * sample_rate_));
    pri_samples_ = static_cast<size_t>(std::round(pri_sec_ * sample_rate_));
    phase_ = 0.0f;
    samples_generated_ = 0;
    pulse_done_ = false;
}

size_t PulseSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (pulse_done_)
        return 0;

    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_generated_ >= total_samples)
            return 0;
        total_available = total_samples - samples_generated_;
    }

    size_t to_generate = std::min(max_samples, total_available);

    const float phase_inc = static_cast<float>(2.0 * M_PI * frequency_hz_ / sample_rate_);
    const float amp = static_cast<float>(amplitude_);
    const float two_pi = static_cast<float>(2.0 * M_PI);

    bool is_single = (mode_ == "single");

    for (size_t i = 0; i < to_generate; ++i) {
        size_t sample_in_pri = (samples_generated_ + i) % pri_samples_;

        if (sample_in_pri < pw_samples_) {
            out[i] = amp * std::complex<float>(std::cos(phase_), std::sin(phase_));
        } else {
            out[i] = std::complex<float>(0.0f, 0.0f);
        }

        phase_ += phase_inc;
        if (phase_ >= two_pi) phase_ -= two_pi;
    }

    samples_generated_ += to_generate;

    if (is_single && samples_generated_ >= pri_samples_) {
        pulse_done_ = true;
    }

    return to_generate;
}

WaveformMetadata PulseSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    meta.peak_amplitude = amplitude_;
    meta.nominal_bandwidth = (pulse_width_sec_ > 0.0) ? 2.0 / pulse_width_sec_ : 0.0;
    meta.duration_sec = duration_sec_;
    meta.repeats = (mode_ == "train") && !duration_sec_.has_value();
    if (pw_samples_ > 0 && pri_samples_ > 0) {
        meta.crest_factor = std::sqrt(static_cast<double>(pri_samples_) / static_cast<double>(pw_samples_));
    }
    if (pri_samples_ > 0) {
        double duty_cycle = static_cast<double>(pw_samples_) / static_cast<double>(pri_samples_);
        meta.rms_amplitude = amplitude_ * std::sqrt(duty_cycle);
    }
    return meta;
}

void PulseSource::reset() {
    phase_ = 0.0f;
    samples_generated_ = 0;
    pulse_done_ = false;
}

} // namespace archerfish::dsp
