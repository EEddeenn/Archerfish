#include "archerfish/dsp/fsk_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void FskSource::configure(const nlohmann::json& params) {
    double next_center_frequency_hz = center_frequency_hz_;
    double next_symbol_rate = symbol_rate_;
    int next_modulation_order = modulation_order_;
    double next_deviation_hz = deviation_hz_;

    if (params.contains("center_frequency_hz")) {
        next_center_frequency_hz = number_param(params, "center_frequency_hz");
        if (!std::isfinite(next_center_frequency_hz)) {
            throw std::invalid_argument("center_frequency_hz must be finite");
        }
    }
    if (params.contains("symbol_rate")) {
        next_symbol_rate = number_param(params, "symbol_rate");
        validate_positive(next_symbol_rate, "symbol_rate");
    }
    if (params.contains("modulation_order")) {
        next_modulation_order = int_param(params, "modulation_order");
        if (next_modulation_order < 2) {
            throw std::invalid_argument("modulation_order must be at least 2");
        }
        if (next_modulation_order > 1024) {
            throw std::invalid_argument("modulation_order must be <= 1024");
        }
    }
    if (params.contains("deviation_hz")) {
        next_deviation_hz = number_param(params, "deviation_hz");
        validate_non_negative(next_deviation_hz, "deviation_hz");
    }

    configure_common(params);
    center_frequency_hz_ = next_center_frequency_hz;
    symbol_rate_ = next_symbol_rate;
    modulation_order_ = next_modulation_order;
    deviation_hz_ = next_deviation_hz;
    samples_per_symbol_ = 0;
    samples_within_symbol_ = 0;
    current_freq_hz_ = 0.0;
    phase_ = 0.0;
}

void FskSource::prepare() {
    const double samples_per_symbol = sample_rate_ / symbol_rate_;
    if (samples_per_symbol < 1.0) {
        throw std::invalid_argument("symbol_rate is too high for sample_rate");
    }
    samples_per_symbol_ = checked_positive_rounded_count(samples_per_symbol, "samples_per_symbol");
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
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("FskSource render output buffer must not be null");
    }
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;
    if (samples_per_symbol_ == 0) {
        throw std::logic_error("FskSource must be prepared before rendering");
    }

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
    meta.rms_amplitude = amplitude_;
    meta.crest_factor = amplitude_ > 0.0 ? 1.0 : 0.0;
    int M = modulation_order_;
    double freq_span = (M > 1) ? 2.0 * deviation_hz_ : 0.0;
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
