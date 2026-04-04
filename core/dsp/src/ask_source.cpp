#include "archerfish/dsp/ask_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void AskSource::configure(const nlohmann::json& params) {
    configure_common(params);
    if (params.contains("frequency_hz"))
        frequency_hz_ = params["frequency_hz"].get<double>();
    if (params.contains("symbol_rate"))
        symbol_rate_ = params["symbol_rate"].get<double>();
    if (params.contains("num_levels"))
        num_levels_ = params["num_levels"].get<int>();
}

void AskSource::prepare() {
    samples_per_symbol_ = static_cast<size_t>(std::round(sample_rate_ / symbol_rate_));
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
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;

    const double phase_inc = archerfish::constants::kTwoPi * frequency_hz_ / sample_rate_;
    const double two_pi = archerfish::constants::kTwoPi;

    for (size_t i = 0; i < to_generate; ++i) {
        float amp = current_symbol_value_;
        out[i] = amp * std::complex<float>(static_cast<float>(std::cos(phase_)), static_cast<float>(std::sin(phase_)));

        phase_ += phase_inc;
        if (phase_ >= two_pi) phase_ -= two_pi;

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
    meta.nominal_bandwidth = 0.0;

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
