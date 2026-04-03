#include "archerfish/dsp/fsk_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace archerfish::dsp {

void FskSource::configure(const nlohmann::json& params) {
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("center_frequency_hz"))
        center_frequency_hz_ = params["center_frequency_hz"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("symbol_rate"))
        symbol_rate_ = params["symbol_rate"].get<double>();
    if (params.contains("modulation_order"))
        modulation_order_ = params["modulation_order"].get<int>();
    if (params.contains("deviation_hz"))
        deviation_hz_ = params["deviation_hz"].get<double>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
    if (params.contains("seed"))
        seed_ = params["seed"].get<uint32_t>();
}

void FskSource::prepare() {
    samples_per_symbol_ = static_cast<size_t>(std::round(sample_rate_ / symbol_rate_));
    rng_.seed(seed_);
    phase_ = 0.0;
    samples_generated_ = 0;
    samples_within_symbol_ = 0;
    generate_next_symbol();
}

void FskSource::generate_next_symbol() {
    int M = modulation_order_;
    int s = static_cast<int>(rng_() % static_cast<unsigned>(M));

    double offset;
    if (M == 2) {
        offset = (s == 0) ? -deviation_hz_ : deviation_hz_;
    } else {
        offset = (2 * s - M + 1) * deviation_hz_ / (M - 1);
    }
    current_freq_hz_ = center_frequency_hz_ + offset;
}

size_t FskSource::render_block(std::complex<float>* out, size_t max_samples) {
    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_generated_ >= total_samples)
            return 0;
        total_available = total_samples - samples_generated_;
    }

    size_t to_generate = std::min(max_samples, total_available);

    const float amp = static_cast<float>(amplitude_);
    const double two_pi = 2.0 * M_PI;

    for (size_t i = 0; i < to_generate; ++i) {
        phase_ += two_pi * current_freq_hz_ / sample_rate_;

        phase_ = std::fmod(phase_, two_pi);
        if (phase_ < 0.0) phase_ += two_pi;

        out[i] = amp * std::complex<float>(static_cast<float>(std::cos(phase_)),
                                            static_cast<float>(std::sin(phase_)));

        samples_within_symbol_++;
        if (samples_within_symbol_ >= samples_per_symbol_) {
            samples_within_symbol_ = 0;
            generate_next_symbol();
        }
    }

    samples_generated_ += to_generate;
    return to_generate;
}

WaveformMetadata FskSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    meta.duration_sec = duration_sec_;
    meta.repeats = !duration_sec_.has_value();
    int M = modulation_order_;
    double freq_span = (M > 1) ? 2.0 * deviation_hz_ * (M - 1) / (M - 1) : 0.0;
    meta.nominal_bandwidth = freq_span + symbol_rate_;
    return meta;
}

void FskSource::reset() {
    rng_.seed(seed_);
    phase_ = 0.0;
    samples_generated_ = 0;
    samples_within_symbol_ = 0;
    generate_next_symbol();
}

} // namespace archerfish::dsp
