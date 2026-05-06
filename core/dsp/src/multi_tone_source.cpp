#include "archerfish/dsp/multi_tone_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void MultiToneSource::configure(const nlohmann::json& params) {
    std::vector<ToneSpec> next_tones = tones_;
    if (params.contains("tones")) {
        if (!params["tones"].is_array()) {
            throw std::invalid_argument("tones must be an array");
        }
        next_tones.clear();
        for (const auto& t : params["tones"]) {
            if (!t.is_object()) {
                throw std::invalid_argument("each tone must be an object");
            }
            ToneSpec spec;
            spec.frequency_hz = t.contains("frequency_hz") ? number_param(t, "frequency_hz") : 0.0;
            spec.amplitude = t.contains("amplitude") ? number_param(t, "amplitude") : 0.2;
            if (!std::isfinite(spec.frequency_hz)) {
                throw std::invalid_argument("tone frequency_hz must be finite");
            }
            validate_non_negative(spec.amplitude, "tone amplitude");
            if (spec.amplitude > static_cast<double>(std::numeric_limits<float>::max())) {
                throw std::out_of_range("tone amplitude exceeds float range");
            }
            next_tones.push_back(spec);
        }
    }

    long double peak_amplitude = 0.0L;
    for (const auto& tone : next_tones) {
        peak_amplitude += static_cast<long double>(tone.amplitude);
        if (peak_amplitude > static_cast<long double>(std::numeric_limits<float>::max())) {
            throw std::out_of_range("multi-tone peak amplitude exceeds float range");
        }
    }

    configure_common(params);
    tones_ = std::move(next_tones);
}

void MultiToneSource::prepare() {
    reset_common();
}

size_t MultiToneSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("MultiToneSource render output buffer must not be null");
    }
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;

    for (size_t i = 0; i < to_generate; ++i) {
        float re = 0.0f;
        float im = 0.0f;
        size_t n = samples_produced_ + i;
        for (const auto& tone : tones_) {
            double phase = archerfish::constants::kTwoPi * tone.frequency_hz / sample_rate_ * static_cast<double>(n);
            float amp = static_cast<float>(tone.amplitude);
            re += amp * static_cast<float>(std::cos(phase));
            im += amp * static_cast<float>(std::sin(phase));
        }
        out[i] = std::complex<float>(re, im);
    }

    samples_produced_ += to_generate;
    return to_generate;
}

WaveformMetadata MultiToneSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);

    double total_amp = 0.0;
    double sum_amp_sq = 0.0;
    for (const auto& t : tones_) {
        total_amp += t.amplitude;
        sum_amp_sq += t.amplitude * t.amplitude;
    }
    meta.peak_amplitude = total_amp;
    meta.rms_amplitude = std::sqrt(sum_amp_sq);
    meta.crest_factor = meta.rms_amplitude > 0.0 ? meta.peak_amplitude / meta.rms_amplitude : 0.0;

    double fmin = std::numeric_limits<double>::max();
    double fmax = std::numeric_limits<double>::lowest();
    for (const auto& t : tones_) {
        fmin = std::min(fmin, t.frequency_hz);
        fmax = std::max(fmax, t.frequency_hz);
    }
    meta.nominal_bandwidth = tones_.empty() ? 0.0 : fmax - fmin;
    return meta;
}

void MultiToneSource::reset() {
    reset_common();
}

} // namespace archerfish::dsp
