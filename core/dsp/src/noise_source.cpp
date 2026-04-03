#include "archerfish/dsp/noise_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace archerfish::dsp {

void NoiseSource::configure(const nlohmann::json& params) {
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
    if (params.contains("seed"))
        seed_ = params["seed"].get<uint32_t>();
}

void NoiseSource::prepare() {
    rng_.seed(seed_);
    dist_ = std::normal_distribution<float>(0.0f, static_cast<float>(amplitude_));
    samples_generated_ = 0;
}

size_t NoiseSource::render_block(std::complex<float>* out, size_t max_samples) {
    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_generated_ >= total_samples)
            return 0;
        total_available = total_samples - samples_generated_;
    }

    size_t to_generate = std::min(max_samples, total_available);

    for (size_t i = 0; i < to_generate; ++i) {
        float re = dist_(rng_);
        float im = dist_(rng_);
        out[i] = std::complex<float>(re, im);
    }

    samples_generated_ += to_generate;
    return to_generate;
}

WaveformMetadata NoiseSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    meta.peak_amplitude = amplitude_ * 4.0f;
    meta.rms_amplitude = amplitude_;
    meta.crest_factor = 4.0;
    meta.duration_sec = duration_sec_;
    meta.repeats = !duration_sec_.has_value();
    meta.nominal_bandwidth = sample_rate_ / 2.0;
    return meta;
}

void NoiseSource::reset() {
    rng_.seed(seed_);
    samples_generated_ = 0;
}

} // namespace archerfish::dsp
