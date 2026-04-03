#include "archerfish/dsp/multi_tone_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace archerfish::dsp {

void MultiToneSource::configure(const nlohmann::json& params) {
    if (params.contains("tones")) {
        tones_.clear();
        for (const auto& t : params["tones"]) {
            ToneSpec spec;
            spec.frequency_hz = t.value("frequency_hz", 0.0);
            spec.amplitude = t.value("amplitude", 0.2);
            tones_.push_back(spec);
        }
    }
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
}

void MultiToneSource::prepare() {
    samples_generated_ = 0;
}

size_t MultiToneSource::render_block(std::complex<float>* out, size_t max_samples) {
    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_generated_ >= total_samples)
            return 0;
        total_available = total_samples - samples_generated_;
    }

    size_t to_generate = std::min(max_samples, total_available);

    for (size_t i = 0; i < to_generate; ++i) {
        float re = 0.0f;
        float im = 0.0f;
        size_t n = samples_generated_ + i;
        for (const auto& tone : tones_) {
            double phase = 2.0 * M_PI * tone.frequency_hz / sample_rate_ * static_cast<double>(n);
            float amp = static_cast<float>(tone.amplitude);
            re += amp * static_cast<float>(std::cos(phase));
            im += amp * static_cast<float>(std::sin(phase));
        }
        out[i] = std::complex<float>(re, im);
    }

    samples_generated_ += to_generate;
    return to_generate;
}

WaveformMetadata MultiToneSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;

    double total_amp = 0.0;
    for (const auto& t : tones_)
        total_amp += t.amplitude;
    meta.peak_amplitude = total_amp;
    meta.rms_amplitude = total_amp / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    meta.duration_sec = duration_sec_;
    meta.repeats = !duration_sec_.has_value();

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
    samples_generated_ = 0;
}

} // namespace archerfish::dsp
