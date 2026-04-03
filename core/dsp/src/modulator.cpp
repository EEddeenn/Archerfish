#include "archerfish/dsp/modulator.hpp"
#include "archerfish/dsp/pulse_shaper.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace archerfish::dsp {

void ModulatorSource::build_constellation() {
    constellation_.clear();
    switch (modulation_) {
    case ModulationType::BPSK:
        constellation_ = {{1.0f, 0.0f}, {-1.0f, 0.0f}};
        break;
    case ModulationType::QPSK: {
        float s = 1.0f / std::sqrt(2.0f);
        constellation_ = {{s, s}, {-s, s}, {-s, -s}, {s, -s}};
        break;
    }
    case ModulationType::PSK8: {
        for (int k = 0; k < 8; ++k) {
            float angle = static_cast<float>(k) * M_PI / 4.0f;
            constellation_.push_back({std::cos(angle), std::sin(angle)});
        }
        break;
    }
    case ModulationType::QAM16: {
        for (int iy = 0; iy < 4; ++iy) {
            for (int ix = 0; ix < 4; ++ix) {
                float re = (static_cast<float>(ix) - 1.5f) / 1.5f;
                float im = (static_cast<float>(iy) - 1.5f) / 1.5f;
                constellation_.push_back({re, im});
            }
        }
        break;
    }
    case ModulationType::QAM64: {
        for (int iy = 0; iy < 8; ++iy) {
            for (int ix = 0; ix < 8; ++ix) {
                float re = (static_cast<float>(ix) - 3.5f) / 3.5f;
                float im = (static_cast<float>(iy) - 3.5f) / 3.5f;
                constellation_.push_back({re, im});
            }
        }
        break;
    }
    }
}

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
    }
    if (params.contains("symbol_rate"))
        symbol_rate_ = params["symbol_rate"].get<double>();
    if (params.contains("samples_per_symbol"))
        samples_per_symbol_ = params["samples_per_symbol"].get<size_t>();
    if (params.contains("rrc_alpha"))
        rrc_alpha_ = params["rrc_alpha"].get<double>();
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
    if (params.contains("seed"))
        seed_ = params["seed"].get<uint32_t>();
}

void ModulatorSource::prepare() {
    rng_.seed(seed_);
    build_constellation();
    build_rrc_taps();
    symbol_buffer_.clear();
    shaped_buffer_.clear();
    output_offset_ = 0;
    samples_produced_ = 0;
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

            size_t upsampled_len = num_symbols * samples_per_symbol_ + rrc_taps_.size() - 1;
            std::vector<std::complex<float>> upsampled(upsampled_len, {0.0f, 0.0f});
            for (size_t i = 0; i < num_symbols; ++i) {
                upsampled[i * samples_per_symbol_] = symbols[i];
            }

            shaped_buffer_.assign(upsampled_len, {0.0f, 0.0f});
            for (size_t n = 0; n < upsampled_len; ++n) {
                std::complex<float> acc{0.0f, 0.0f};
                size_t k_min = (n >= rrc_taps_.size() - 1) ? n - (rrc_taps_.size() - 1) : 0;
                size_t k_max = std::min(n, upsampled_len - 1);
                for (size_t k = k_min; k <= k_max; ++k) {
                    size_t tap_idx = n - k;
                    if (tap_idx < rrc_taps_.size()) {
                        acc += upsampled[k] * rrc_taps_[tap_idx];
                    }
                }
                shaped_buffer_[n] = acc;
            }
            output_offset_ = 0;
        }
    }

    samples_produced_ += produced;
    return produced;
}

WaveformMetadata ModulatorSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    meta.duration_sec = duration_sec_;
    meta.repeats = !duration_sec_.has_value();
    meta.nominal_bandwidth = symbol_rate_ * (1.0 + rrc_alpha_);
    return meta;
}

void ModulatorSource::reset() {
    rng_.seed(seed_);
    symbol_buffer_.clear();
    shaped_buffer_.clear();
    output_offset_ = 0;
    samples_produced_ = 0;
}

} // namespace archerfish::dsp
