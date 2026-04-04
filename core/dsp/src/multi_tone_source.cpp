#include "archerfish/dsp/multi_tone_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void MultiToneSource::configure(const nlohmann::json& params) {
    configure_common(params);
    if (params.contains("tones")) {
        tones_.clear();
        for (const auto& t : params["tones"]) {
            ToneSpec spec;
            spec.frequency_hz = t.value("frequency_hz", 0.0);
            spec.amplitude = t.value("amplitude", 0.2);
            tones_.push_back(spec);
        }
    }
}

void MultiToneSource::prepare() {
    reset_common();
}

size_t MultiToneSource::render_block(std::complex<float>* out, size_t max_samples) {
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
    for (const auto& t : tones_)
        total_amp += t.amplitude;
    meta.peak_amplitude = total_amp;
    meta.rms_amplitude = total_amp / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);

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
