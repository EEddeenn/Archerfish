#include "archerfish/dsp/ofdm_source.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace archerfish::dsp {

void OfdmSource::bit_reversal_permute(std::vector<std::complex<float>>& data) {
    auto n = data.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }
}

void OfdmSource::fft_in_place(std::vector<std::complex<float>>& data, bool inverse) {
    auto n = data.size();
    if (n <= 1) return;

    bit_reversal_permute(data);

    for (size_t len = 2; len <= n; len <<= 1) {
        double angle = (inverse ? -1.0 : 1.0) * 2.0 * std::numbers::pi / static_cast<double>(len);
        std::complex<float> wlen{static_cast<float>(std::cos(angle)),
                                  static_cast<float>(std::sin(angle))};
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w{1.0f, 0.0f};
            for (size_t j = 0; j < len / 2; ++j) {
                auto u = data[i + j];
                auto v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse) {
        float inv_n = 1.0f / static_cast<float>(n);
        for (auto& x : data) {
            x *= inv_n;
        }
    }
}

void OfdmSource::generate_ofdm_symbol() {
    std::vector<std::complex<float>> freq_domain(fft_size_, {0.0f, 0.0f});

    size_t half = active_subcarriers_ / 2;
    for (size_t k = 0; k < active_subcarriers_; ++k) {
        size_t bin;
        if (k < half) {
            bin = fft_size_ - half + k;
        } else {
            bin = k - half;
        }

        uint32_t bits = static_cast<uint32_t>(rng_()) & 0x3u;
        float re = (bits & 1) ? 1.0f : -1.0f;
        float im = (bits & 2) ? 1.0f : -1.0f;
        float norm = 1.0f / std::sqrt(2.0f);
        freq_domain[bin] = std::complex<float>{re * norm, im * norm};
    }

    fft_in_place(freq_domain, true);

    symbol_buffer_.clear();
    symbol_buffer_.reserve(fft_size_ + cyclic_prefix_size_);

    size_t cp_start = fft_size_ - cyclic_prefix_size_;
    for (size_t i = 0; i < cyclic_prefix_size_; ++i) {
        symbol_buffer_.push_back(freq_domain[cp_start + i]);
    }
    for (size_t i = 0; i < fft_size_; ++i) {
        symbol_buffer_.push_back(freq_domain[i]);
    }
}

void OfdmSource::configure(const nlohmann::json& params) {
    configure_common(params);
    if (params.contains("fft_size"))
        fft_size_ = params["fft_size"].get<size_t>();
    if (params.contains("cyclic_prefix_size"))
        cyclic_prefix_size_ = params["cyclic_prefix_size"].get<size_t>();
    if (params.contains("active_subcarriers"))
        active_subcarriers_ = params["active_subcarriers"].get<size_t>();
}

void OfdmSource::prepare() {
    rng_.seed(seed());
    symbol_buffer_.clear();
    output_offset_ = 0;
    reset_common();
}

size_t OfdmSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_produced_ >= total_samples)
            return 0;
        max_samples = std::min(max_samples, total_samples - samples_produced_);
    }

    size_t produced = 0;
    while (produced < max_samples) {
        if (output_offset_ < symbol_buffer_.size()) {
            size_t remaining = symbol_buffer_.size() - output_offset_;
            size_t to_copy = std::min(remaining, max_samples - produced);
            for (size_t i = 0; i < to_copy; ++i) {
                out[produced + i] = symbol_buffer_[output_offset_ + i] * static_cast<float>(amplitude_);
            }
            output_offset_ += to_copy;
            produced += to_copy;
        } else {
            generate_ofdm_symbol();
            output_offset_ = 0;
        }
    }

    samples_produced_ += produced;
    return produced;
}

WaveformMetadata OfdmSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_;
    meta.nominal_bandwidth = sample_rate_;
    return meta;
}

void OfdmSource::reset() {
    rng_.seed(seed());
    symbol_buffer_.clear();
    output_offset_ = 0;
    reset_common();
}

} // namespace archerfish::dsp
