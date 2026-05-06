#include "archerfish/dsp/noise_source.hpp"

#include <cmath>
#include <stdexcept>

namespace archerfish::dsp {

void NoiseSource::configure(const nlohmann::json& params) {
    configure_common(params);
    rng_.seed(seed());
    dist_ = std::normal_distribution<float>(0.0f, static_cast<float>(amplitude()));
}

void NoiseSource::prepare() {
    rng_.seed(seed());
    dist_ = std::normal_distribution<float>(0.0f, static_cast<float>(amplitude()));
    reset_common();
}

size_t NoiseSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("NoiseSource render output buffer must not be null");
    }
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;

    for (size_t i = 0; i < to_generate; ++i) {
        float re = dist_(rng_);
        float im = dist_(rng_);
        out[i] = std::complex<float>(re, im);
    }

    samples_produced_ += to_generate;
    return to_generate;
}

WaveformMetadata NoiseSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude() * 4.0f;
    meta.rms_amplitude = amplitude();
    meta.crest_factor = 4.0;
    meta.nominal_bandwidth = sample_rate() / 2.0;
    return meta;
}

void NoiseSource::reset() {
    rng_.seed(seed());
    reset_common();
}

} // namespace archerfish::dsp
