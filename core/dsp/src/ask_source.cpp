#include "archerfish/dsp/ask_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace archerfish::dsp {

void AskSource::configure(const nlohmann::json& params) {
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("frequency_hz"))
        frequency_hz_ = params["frequency_hz"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("symbol_rate"))
        symbol_rate_ = params["symbol_rate"].get<double>();
    if (params.contains("num_levels"))
        num_levels_ = params["num_levels"].get<int>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
    if (params.contains("seed"))
        seed_ = params["seed"].get<uint32_t>();
}

void AskSource::prepare() {
    samples_per_symbol_ = static_cast<size_t>(std::round(sample_rate_ / symbol_rate_));
    rng_.seed(seed_);
    phase_ = 0.0f;
    samples_generated_ = 0;
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
    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_generated_ >= total_samples)
            return 0;
        total_available = total_samples - samples_generated_;
    }

    size_t to_generate = std::min(max_samples, total_available);

    const float phase_inc = static_cast<float>(2.0 * M_PI * frequency_hz_ / sample_rate_);
    const float two_pi = static_cast<float>(2.0 * M_PI);

    for (size_t i = 0; i < to_generate; ++i) {
        float amp = current_symbol_value_;
        out[i] = amp * std::complex<float>(std::cos(phase_), std::sin(phase_));

        phase_ += phase_inc;
        if (phase_ > two_pi) phase_ -= two_pi;

        samples_within_symbol_++;
        if (samples_within_symbol_ >= samples_per_symbol_) {
            samples_within_symbol_ = 0;
            generate_next_symbol();
        }
    }

    samples_generated_ += to_generate;
    return to_generate;
}

WaveformMetadata AskSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    meta.duration_sec = duration_sec_;
    meta.repeats = !duration_sec_.has_value();
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
    rng_.seed(seed_);
    phase_ = 0.0f;
    samples_generated_ = 0;
    samples_within_symbol_ = 0;
    generate_next_symbol();
}

} // namespace archerfish::dsp
