#include "archerfish/dsp/fsk_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void FskSource::configure(const nlohmann::json& params) {
    configure_common(params);
    if (params.contains("center_frequency_hz"))
        center_frequency_hz_ = params["center_frequency_hz"].get<double>();
    if (params.contains("symbol_rate"))
        symbol_rate_ = params["symbol_rate"].get<double>();
    if (params.contains("modulation_order"))
        modulation_order_ = params["modulation_order"].get<int>();
    if (params.contains("deviation_hz"))
        deviation_hz_ = params["deviation_hz"].get<double>();
}

void FskSource::prepare() {
    samples_per_symbol_ = static_cast<size_t>(std::round(sample_rate_ / symbol_rate_));
    rng_.seed(seed());
    phase_ = 0.0;
    reset_common();
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
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;

    const float amp = static_cast<float>(amplitude_);
    const double two_pi = archerfish::constants::kTwoPi;

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

    samples_produced_ += to_generate;
    return to_generate;
}

WaveformMetadata FskSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    int M = modulation_order_;
    double freq_span = (M > 1) ? 2.0 * deviation_hz_ * (M - 1) / (M - 1) : 0.0;
    meta.nominal_bandwidth = freq_span + symbol_rate_;
    return meta;
}

void FskSource::reset() {
    rng_.seed(seed());
    reset_common();
    phase_ = 0.0;
    samples_within_symbol_ = 0;
    generate_next_symbol();
}

} // namespace archerfish::dsp
