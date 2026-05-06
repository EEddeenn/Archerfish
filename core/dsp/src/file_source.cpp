#include "archerfish/dsp/file_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

#include <spdlog/spdlog.h>

namespace archerfish::dsp {

namespace {

constexpr size_t kMaxFileSourceSamples = 16'000'000;

int16_t float_to_ci16(float value) {
    if (!std::isfinite(value)) {
        value = 0.0f;
    }
    return static_cast<int16_t>(std::clamp(value, -1.0f, 1.0f) * 32767.0f);
}

bool exceeds_file_source_sample_limit(std::streamoff byte_size, size_t bytes_per_sample) {
    if (byte_size < 0) {
        return true;
    }
    return static_cast<std::uintmax_t>(byte_size) / bytes_per_sample > kMaxFileSourceSamples;
}

} // namespace

FileFormat detect_file_format(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".ci16") {
        return FileFormat::CI16;
    }
    return FileFormat::CF32;
}

void write_ci16(const std::string& path, const std::vector<std::complex<float>>& samples) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot open CI16 output file: " + path);
    }
    for (const auto& s : samples) {
        int16_t re = float_to_ci16(s.real());
        int16_t im = float_to_ci16(s.imag());
        out.write(reinterpret_cast<const char*>(&re), sizeof(int16_t));
        out.write(reinterpret_cast<const char*>(&im), sizeof(int16_t));
        if (!out) {
            throw std::runtime_error("Failed while writing CI16 output file: " + path);
        }
    }
}

void FileSource::configure(const nlohmann::json& params) {
    std::string next_path = path_;
    bool next_loop = loop_;

    if (params.contains("path")) {
        next_path = string_param(params, "path");
        if (next_path.empty()) {
            throw std::invalid_argument("path must not be empty");
        }
    }
    if (params.contains("loop"))
        next_loop = bool_param(params, "loop");

    configure_common(params);
    path_ = std::move(next_path);
    loop_ = next_loop;
    data_.clear();
    read_offset_ = 0;
    eof_ = false;
}

void FileSource::prepare() {
    data_.clear();
    read_offset_ = 0;
    eof_ = false;
    reset_common();

    std::ifstream file(path_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::warn("FileSource: cannot open file '{}'", path_);
        return;
    }

    auto size = file.tellg();
    if (size <= 0) {
        spdlog::warn("FileSource: empty or invalid file '{}'", path_);
        return;
    }
    file.seekg(0, std::ios::beg);

    auto fmt = detect_file_format(path_);
    if (fmt == FileFormat::CI16) {
        if (size % static_cast<std::streamoff>(2 * sizeof(int16_t)) != 0) {
            spdlog::warn("FileSource: CI16 file '{}' has a partial sample", path_);
            eof_ = true;
            return;
        }
        if (exceeds_file_source_sample_limit(size, 2 * sizeof(int16_t))) {
            spdlog::warn("FileSource: CI16 file '{}' exceeds maximum supported sample count ({})",
                         path_, kMaxFileSourceSamples);
            eof_ = true;
            return;
        }
        size_t num_samples = static_cast<size_t>(size) / (2 * sizeof(int16_t));
        std::vector<int16_t> raw(num_samples * 2);
        file.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size() * sizeof(int16_t)));
        if (!file) {
            spdlog::warn("FileSource: failed while reading CI16 file '{}'", path_);
            data_.clear();
            eof_ = true;
            return;
        }
        data_.resize(num_samples);
        for (size_t i = 0; i < num_samples; ++i) {
            float re = static_cast<float>(raw[i * 2]) / 32767.0f;
            float im = static_cast<float>(raw[i * 2 + 1]) / 32767.0f;
            data_[i] = std::complex<float>(re, im);
        }
    } else {
        if (size % static_cast<std::streamoff>(sizeof(std::complex<float>)) != 0) {
            spdlog::warn("FileSource: CF32 file '{}' has a partial sample", path_);
            eof_ = true;
            return;
        }
        if (exceeds_file_source_sample_limit(size, sizeof(std::complex<float>))) {
            spdlog::warn("FileSource: CF32 file '{}' exceeds maximum supported sample count ({})",
                         path_, kMaxFileSourceSamples);
            eof_ = true;
            return;
        }
        size_t num_samples = static_cast<size_t>(size) / sizeof(std::complex<float>);
        data_.resize(num_samples);
        file.read(reinterpret_cast<char*>(data_.data()), static_cast<std::streamsize>(num_samples * sizeof(std::complex<float>)));
        if (!file) {
            spdlog::warn("FileSource: failed while reading CF32 file '{}'", path_);
            data_.clear();
            eof_ = true;
            return;
        }
    }

    if (data_.empty()) {
        eof_ = true;
    }
}

size_t FileSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("FileSource render output buffer must not be null");
    }
    if (data_.empty() || eof_)
        return 0;

    size_t total_limit = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        total_limit = checked_sample_count(sample_rate_, duration_sec_.value());
        if (samples_produced_ >= total_limit) {
            eof_ = true;
            return 0;
        }
    }

    size_t produced = 0;
    size_t effective_max = max_samples;
    if (duration_sec_.has_value()) {
        effective_max = std::min(max_samples, total_limit - samples_produced_);
    }

    for (size_t i = 0; i < effective_max; ++i) {
        if (read_offset_ >= data_.size()) {
            if (loop_) {
                read_offset_ = 0;
            } else {
                eof_ = true;
                break;
            }
        }

        out[i] = data_[read_offset_];
        ++read_offset_;
        ++produced;
        ++samples_produced_;
    }

    if (duration_sec_.has_value() && samples_produced_ >= total_limit) {
        eof_ = true;
    }

    return produced;
}

WaveformMetadata FileSource::report_metadata() const {
    WaveformMetadata meta;
    meta.sample_rate = sample_rate_;
    if (!data_.empty()) {
        float max_mag = 0.0f;
        double sum_mag_sq = 0.0;
        size_t finite_count = 0;
        for (const auto& s : data_) {
            float mag = std::abs(s);
            if (!std::isfinite(mag)) continue;
            max_mag = std::max(max_mag, mag);
            sum_mag_sq += static_cast<double>(mag) * static_cast<double>(mag);
            ++finite_count;
        }
        meta.peak_amplitude = static_cast<double>(max_mag);
        meta.rms_amplitude = finite_count > 0 ? std::sqrt(sum_mag_sq / static_cast<double>(finite_count)) : 0.0;
        meta.crest_factor = (meta.rms_amplitude > 0.0) ? meta.peak_amplitude / meta.rms_amplitude : 0.0;
    }
    meta.duration_sec = duration_sec_;
    meta.repeats = loop_ && !duration_sec_.has_value();
    return meta;
}

void FileSource::reset() {
    reset_common();
    read_offset_ = 0;
    eof_ = data_.empty();
}

} // namespace archerfish::dsp
