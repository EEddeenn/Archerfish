#include "archerfish/dsp/pulse_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void PulseSource::configure(const nlohmann::json& params) {
    double next_frequency_hz = frequency_hz_;
    double next_pulse_width_sec = pulse_width_sec_;
    double next_pri_sec = pri_sec_;
    std::string next_mode = mode_;

    if (params.contains("frequency_hz")) {
        next_frequency_hz = number_param(params, "frequency_hz");
        if (!std::isfinite(next_frequency_hz)) {
            throw std::invalid_argument("frequency_hz must be finite");
        }
    }
    if (params.contains("pulse_width_sec")) {
        next_pulse_width_sec = number_param(params, "pulse_width_sec");
        validate_positive(next_pulse_width_sec, "pulse_width_sec");
    }
    if (params.contains("pri_sec")) {
        next_pri_sec = number_param(params, "pri_sec");
        validate_positive(next_pri_sec, "pri_sec");
    }
    if (params.contains("mode"))
        next_mode = string_param(params, "mode");

    if (next_pulse_width_sec > next_pri_sec) {
        throw std::invalid_argument("pulse_width_sec must not exceed pri_sec");
    }
    if (next_mode != "single" && next_mode != "train") {
        throw std::invalid_argument("mode must be 'single' or 'train'");
    }

    configure_common(params);
    frequency_hz_ = next_frequency_hz;
    pulse_width_sec_ = next_pulse_width_sec;
    pri_sec_ = next_pri_sec;
    mode_ = std::move(next_mode);
    pw_samples_ = 0;
    pri_samples_ = 0;
    phase_ = 0.0f;
    pulse_done_ = false;
}

void PulseSource::prepare() {
    pw_samples_ = checked_sample_count(sample_rate_, pulse_width_sec_);
    pri_samples_ = checked_sample_count(sample_rate_, pri_sec_);
    if (pw_samples_ == 0) {
        throw std::invalid_argument("pulse_width_sec is too small for sample_rate");
    }
    if (pri_samples_ == 0) {
        throw std::invalid_argument("pri_sec is too small for sample_rate");
    }
    phase_ = 0.0;
    reset_common();
    pulse_done_ = false;
}

size_t PulseSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("PulseSource render output buffer must not be null");
    }
    if (pulse_done_)
        return 0;
    if (max_samples > 0 && (pw_samples_ == 0 || pri_samples_ == 0)) {
        throw std::logic_error("PulseSource must be prepared before rendering");
    }

    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = checked_sample_count(sample_rate_, duration_sec_.value());
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
        phase_ = std::fmod(phase_, two_pi);
        if (phase_ < 0.0) phase_ += two_pi;
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
