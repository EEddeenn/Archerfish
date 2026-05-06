#include "archerfish/dsp/ask_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void AskSource::configure(const nlohmann::json& params) {
    double next_frequency_hz = frequency_hz_;
    double next_symbol_rate = symbol_rate_;
    double next_amplitude = amplitude_;
    int next_num_levels = num_levels_;

    if (params.contains("amplitude")) {
        next_amplitude = number_param(params, "amplitude");
        validate_non_negative(next_amplitude, "amplitude");
        if (next_amplitude > static_cast<double>(std::numeric_limits<float>::max())) {
            throw std::out_of_range("amplitude exceeds float range");
        }
    }
    if (params.contains("frequency_hz")) {
        next_frequency_hz = number_param(params, "frequency_hz");
        if (!std::isfinite(next_frequency_hz)) {
            throw std::invalid_argument("frequency_hz must be finite");
        }
    }
    if (params.contains("symbol_rate")) {
        next_symbol_rate = number_param(params, "symbol_rate");
        validate_positive(next_symbol_rate, "symbol_rate");
    }
    if (params.contains("num_levels")) {
        next_num_levels = int_param(params, "num_levels");
        if (next_num_levels < 2) {
            throw std::invalid_argument("num_levels must be at least 2");
        }
        if (next_num_levels > 1024) {
            throw std::invalid_argument("num_levels must be <= 1024");
        }
    }

    const double peak_multiplier =
        (next_num_levels == 2)
            ? std::sqrt(2.0)
            : std::sqrt(3.0 / (static_cast<double>(next_num_levels) * next_num_levels - 1.0)) *
                  (next_num_levels - 1);
    if (static_cast<long double>(next_amplitude) * static_cast<long double>(peak_multiplier) >
        static_cast<long double>(std::numeric_limits<float>::max())) {
        throw std::out_of_range("ASK peak amplitude exceeds float range");
    }

    configure_common(params);
    frequency_hz_ = next_frequency_hz;
    symbol_rate_ = next_symbol_rate;
    num_levels_ = next_num_levels;
    samples_per_symbol_ = 0;
    samples_within_symbol_ = 0;
    current_symbol_value_ = 0.0f;
    phase_ = 0.0;
}

void AskSource::prepare() {
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

void AskSource::generate_next_symbol() {
    int M = num_levels_;
    int k = static_cast<int>(rng_() % static_cast<unsigned>(M));

    float alpha;
    if (M == 2) {
        if (k == 0) {
            current_symbol_value_ = 0.0f;
        } else {
            current_symbol_value_ = static_cast<float>(amplitude_ * std::sqrt(2.0));
        }
        return;
    }

    alpha = static_cast<float>(std::sqrt(3.0 / (static_cast<double>(M) * M - 1.0)));
    current_symbol_value_ = static_cast<float>((2 * k - M + 1)) * alpha * static_cast<float>(amplitude_);
}

size_t AskSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("AskSource render output buffer must not be null");
    }
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;
    if (samples_per_symbol_ == 0) {
        throw std::logic_error("AskSource must be prepared before rendering");
    }

    const double phase_inc = archerfish::constants::kTwoPi * frequency_hz_ / sample_rate_;
    const double two_pi = archerfish::constants::kTwoPi;

    for (size_t i = 0; i < to_generate; ++i) {
        float amp = current_symbol_value_;
        out[i] = amp * std::complex<float>(static_cast<float>(std::cos(phase_)), static_cast<float>(std::sin(phase_)));

        phase_ += phase_inc;
        phase_ = std::fmod(phase_, two_pi);
        if (phase_ < 0.0) phase_ += two_pi;

        samples_within_symbol_++;
        if (samples_within_symbol_ >= samples_per_symbol_) {
            samples_within_symbol_ = 0;
            generate_next_symbol();
        }
    }

    samples_produced_ += to_generate;
    return to_generate;
}

WaveformMetadata AskSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.nominal_bandwidth = symbol_rate_;

    int M = num_levels_;
    if (M == 2) {
        meta.peak_amplitude = amplitude_ * std::sqrt(2.0);
        meta.rms_amplitude = amplitude_;
    } else {
        double alpha = std::sqrt(3.0 / (static_cast<double>(M) * M - 1.0));
        meta.peak_amplitude = amplitude_ * alpha * (M - 1);
        double sum_sq = 0.0;
        for (int k = 0; k < M; ++k) {
            double val = (2 * k - M + 1) * alpha * amplitude_;
            sum_sq += val * val;
        }
        meta.rms_amplitude = std::sqrt(sum_sq / M);
    }
    meta.crest_factor = (meta.rms_amplitude > 0.0) ? meta.peak_amplitude / meta.rms_amplitude : 0.0;

    return meta;
}

void AskSource::reset() {
    rng_.seed(seed());
    reset_common();
    phase_ = 0.0;
    samples_within_symbol_ = 0;
    generate_next_symbol();
}

} // namespace archerfish::dsp
