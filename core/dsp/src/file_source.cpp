#include "archerfish/dsp/file_source.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

#include <spdlog/spdlog.h>

namespace archerfish::dsp {

void FileSource::configure(const nlohmann::json& params) {
    if (params.contains("path"))
        path_ = params["path"].get<std::string>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("loop"))
        loop_ = params["loop"].get<bool>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
}

void FileSource::prepare() {
    data_.clear();
    read_offset_ = 0;
    eof_ = false;

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
    size_t num_samples = static_cast<size_t>(size) / sizeof(std::complex<float>);
    data_.resize(num_samples);
    file.read(reinterpret_cast<char*>(data_.data()), static_cast<std::streamsize>(num_samples * sizeof(std::complex<float>)));

    if (data_.empty()) {
        eof_ = true;
    }
}

size_t FileSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (data_.empty() || eof_)
        return 0;

    size_t total_limit = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        total_limit = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
    }

    size_t produced = 0;
    size_t effective_max = std::min(max_samples, total_limit);

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
    }

    if (duration_sec_.has_value() && produced >= total_limit) {
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
        for (const auto& s : data_) {
            float mag = std::abs(s);
            max_mag = std::max(max_mag, mag);
            sum_mag_sq += static_cast<double>(mag) * static_cast<double>(mag);
        }
        meta.peak_amplitude = static_cast<double>(max_mag);
        meta.rms_amplitude = std::sqrt(sum_mag_sq / static_cast<double>(data_.size()));
        meta.crest_factor = (meta.rms_amplitude > 0.0) ? meta.peak_amplitude / meta.rms_amplitude : 0.0;
    }
    meta.duration_sec = duration_sec_;
    meta.repeats = loop_;
    return meta;
}

void FileSource::reset() {
    read_offset_ = 0;
    eof_ = data_.empty();
}

} // namespace archerfish::dsp
