#include "archerfish/dsp/pulse_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void PulseSource::configure(const nlohmann::json& params) {
    configure_common(params);
    if (params.contains("frequency_hz"))
        frequency_hz_ = params["frequency_hz"].get<double>();
    if (params.contains("pulse_width_sec"))
        pulse_width_sec_ = params["pulse_width_sec"].get<double>();
    if (params.contains("pri_sec"))
        pri_sec_ = params["pri_sec"].get<double>();
    if (params.contains("mode"))
        mode_ = params["mode"].get<std::string>();
}

void PulseSource::prepare() {
    pw_samples_ = static_cast<size_t>(std::round(pulse_width_sec_ * sample_rate_));
    pri_samples_ = static_cast<size_t>(std::round(pri_sec_ * sample_rate_));
    phase_ = 0.0;
    reset_common();
    pulse_done_ = false;
}

size_t PulseSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (pulse_done_)
        return 0;

    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_produced_ >= total_samples)
            return 0;
        total_available = total_samples - samples_produced_;
    }

    size_t to_generate = std::min(max_samples, total_available);

    const double phase_inc = archerfish::constants::kTwoPi * frequency_hz_ / sample_rate_;
    const float amp = static_cast<float>(amplitude_);
    const double two_pi = archerfish::constants::kTwoPi;

    bool is_single = (mode_ == "single");

    for (size_t i = 0; i < to_generate; ++i) {
        size_t sample_in_pri = (samples_produced_ + i) % pri_samples_;

        if (sample_in_pri < pw_samples_) {
            out[i] = amp * std::complex<float>(static_cast<float>(std::cos(phase_)), static_cast<float>(std::sin(phase_)));
        } else {
            out[i] = std::complex<float>(0.0f, 0.0f);
        }

        phase_ += phase_inc;
        if (phase_ >= two_pi) phase_ -= two_pi;
    }

    samples_produced_ += to_generate;

    if (is_single && samples_produced_ >= pri_samples_) {
        pulse_done_ = true;
    }

    return to_generate;
}

WaveformMetadata PulseSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_;
    meta.nominal_bandwidth = (pulse_width_sec_ > 0.0) ? 2.0 / pulse_width_sec_ : 0.0;
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
    reset_common();
    phase_ = 0.0;
    pulse_done_ = false;
}

} // namespace archerfish::dsp
