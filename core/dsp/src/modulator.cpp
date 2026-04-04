#include "archerfish/dsp/constellation.hpp"
#include "archerfish/dsp/modulator.hpp"
#include "archerfish/dsp/pulse_shaper.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace archerfish::dsp {

void ModulatorSource::build_rrc_taps() {
    RrcFilterDesign design;
    design.alpha = rrc_alpha_;
    design.span_symbols = 6;
    design.samples_per_symbol = samples_per_symbol_;
    rrc_taps_ = design.design();
}

uint32_t ModulatorSource::random_bits() {
    return static_cast<uint32_t>(rng_());
}

std::complex<float> ModulatorSource::map_symbol(uint32_t bits) {
    size_t idx = bits % constellation_.size();
    return constellation_[idx];
}

void ModulatorSource::configure(const nlohmann::json& params) {
    configure_common(params);
    std::string mod;
    if (params.contains("modulation")) {
        mod = params["modulation"].get<std::string>();
    } else if (params.contains("type")) {
        mod = params["type"].get<std::string>();
    }
    if (!mod.empty()) {
        std::string upper = mod;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        if (upper == "BPSK") modulation_ = ModulationType::BPSK;
        else if (upper == "QPSK") modulation_ = ModulationType::QPSK;
        else if (upper == "PSK8" || upper == "8PSK") modulation_ = ModulationType::PSK8;
        else if (upper == "QAM16" || upper == "16QAM") modulation_ = ModulationType::QAM16;
        else if (upper == "QAM64" || upper == "64QAM") modulation_ = ModulationType::QAM64;
        else if (upper == "APSK16" || upper == "16APSK") modulation_ = ModulationType::APSK16;
        else if (upper == "APSK32" || upper == "32APSK") modulation_ = ModulationType::APSK32;
    }
    if (params.contains("symbol_rate"))
        symbol_rate_ = params["symbol_rate"].get<double>();
    if (params.contains("samples_per_symbol"))
        samples_per_symbol_ = params["samples_per_symbol"].get<size_t>();
    if (params.contains("rrc_alpha"))
        rrc_alpha_ = params["rrc_alpha"].get<double>();
}

void ModulatorSource::prepare() {
    rng_.seed(seed());
    constellation_ = build_constellation(modulation_);
    build_rrc_taps();
    shaped_buffer_.clear();
    filter_tail_.assign(rrc_taps_.size() - 1, {0.0f, 0.0f});
    output_offset_ = 0;
    reset_common();

    constexpr size_t calib_symbols = 256;
    auto saved_rng = rng_;

    size_t bits_per_symbol = 1;
    if (constellation_.size() > 1)
        bits_per_symbol = static_cast<size_t>(std::lround(std::log2(static_cast<double>(constellation_.size()))));

    double mean_symbol_mag = 0.0;
    std::vector<std::complex<float>> symbols(calib_symbols);
    for (size_t i = 0; i < calib_symbols; ++i) {
        uint32_t bits = static_cast<uint32_t>(rng_()) & ((1u << bits_per_symbol) - 1);
        symbols[i] = map_symbol(bits);
        mean_symbol_mag += std::abs(symbols[i]);
    }
    mean_symbol_mag /= static_cast<double>(calib_symbols);

    size_t upsampled_len = calib_symbols * samples_per_symbol_ + rrc_taps_.size() - 1;
    std::vector<std::complex<float>> upsampled(upsampled_len, {0.0f, 0.0f});
    for (size_t i = 0; i < calib_symbols; ++i)
        upsampled[i * samples_per_symbol_] = symbols[i];

    std::vector<std::complex<float>> shaped(upsampled_len, {0.0f, 0.0f});
    for (size_t n = 0; n < upsampled_len; ++n) {
        std::complex<float> acc{0.0f, 0.0f};
        size_t k_min = (n >= rrc_taps_.size() - 1) ? n - (rrc_taps_.size() - 1) : 0;
        size_t k_max = std::min(n, upsampled_len - 1);
        for (size_t k = k_min; k <= k_max; ++k) {
            size_t tap_idx = n - k;
            if (tap_idx < rrc_taps_.size())
                acc += upsampled[k] * rrc_taps_[tap_idx];
        }
        shaped[n] = acc;
    }

    double peak = 0.0;
    double sum_sq = 0.0;
    for (auto& s : shaped) {
        double mag = std::abs(s);
        peak = std::max(peak, mag);
        sum_sq += mag * mag;
    }
    double rms = std::sqrt(sum_sq / static_cast<double>(shaped.size()));

    if (mean_symbol_mag > 0.0 && rms > 0.0) {
        rms_ratio_ = rms / mean_symbol_mag;
        peak_to_rms_ratio_ = peak / rms;
    } else {
        rms_ratio_ = 1.0 / std::sqrt(2.0);
        peak_to_rms_ratio_ = std::sqrt(2.0);
    }

    rng_ = saved_rng;
}

size_t ModulatorSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_produced_ >= total_samples)
            return 0;
        max_samples = std::min(max_samples, total_samples - samples_produced_);
    }

    size_t produced = 0;
    while (produced < max_samples) {
        if (output_offset_ < shaped_buffer_.size()) {
            size_t remaining_in_buffer = shaped_buffer_.size() - output_offset_;
            size_t to_copy = std::min(remaining_in_buffer, max_samples - produced);

            for (size_t i = 0; i < to_copy; ++i) {
                out[produced + i] = shaped_buffer_[output_offset_ + i] * static_cast<float>(amplitude_);
            }
            output_offset_ += to_copy;
            produced += to_copy;
        } else {
            size_t bits_per_symbol = 1;
            if (constellation_.size() > 1)
                bits_per_symbol = static_cast<size_t>(std::lround(std::log2(static_cast<double>(constellation_.size()))));

            size_t num_symbols = 64;
            std::vector<std::complex<float>> symbols(num_symbols);
            for (size_t i = 0; i < num_symbols; ++i) {
                uint32_t bits = random_bits() & ((1u << bits_per_symbol) - 1);
                symbols[i] = map_symbol(bits);
            }

            const size_t N = num_symbols * samples_per_symbol_;
            const size_t L = rrc_taps_.size();
            const size_t tail_len = L - 1;

            size_t upsampled_len = N + tail_len;
            std::vector<std::complex<float>> upsampled(upsampled_len, {0.0f, 0.0f});
            for (size_t i = 0; i < num_symbols; ++i) {
                upsampled[i * samples_per_symbol_] = symbols[i];
            }

            size_t extended_len = tail_len + upsampled_len;
            std::vector<std::complex<float>> extended(extended_len, {0.0f, 0.0f});
            std::copy(filter_tail_.begin(), filter_tail_.end(), extended.begin());
            std::copy(upsampled.begin(), upsampled.end(), extended.begin() + tail_len);

            shaped_buffer_.assign(N, {0.0f, 0.0f});
            for (size_t n = 0; n < N; ++n) {
                size_t ext_n = n + tail_len;
                std::complex<float> acc{0.0f, 0.0f};
                size_t k_min = (ext_n >= tail_len) ? ext_n - tail_len : 0;
                size_t k_max = std::min(ext_n, extended_len - 1);
                for (size_t k = k_min; k <= k_max; ++k) {
                    size_t tap_idx = ext_n - k;
                    if (tap_idx < L) {
                        acc += extended[k] * rrc_taps_[tap_idx];
                    }
                }
                shaped_buffer_[n] = acc;
            }

            filter_tail_.assign(
                upsampled.begin() + (N - tail_len),
                upsampled.begin() + N);
            output_offset_ = 0;
        }
    }

    samples_produced_ += produced;
    return produced;
}

WaveformMetadata ModulatorSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ * rms_ratio_;
    meta.crest_factor = peak_to_rms_ratio_;
    meta.nominal_bandwidth = symbol_rate_ * (1.0 + rrc_alpha_);
    return meta;
}

void ModulatorSource::reset() {
    rng_.seed(seed());
    shaped_buffer_.clear();
    if (!rrc_taps_.empty())
        filter_tail_.assign(rrc_taps_.size() - 1, {0.0f, 0.0f});
    else
        filter_tail_.clear();
    output_offset_ = 0;
    reset_common();
    peak_to_rms_ratio_ = std::sqrt(2.0);
    rms_ratio_ = 1.0 / std::sqrt(2.0);
}

} // namespace archerfish::dsp
