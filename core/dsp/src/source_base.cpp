#include "archerfish/dsp/source_base.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace archerfish::dsp {

void SourceBase::configure_common(const nlohmann::json& params) {
    if (params.contains("amplitude"))
        amplitude_ = params["amplitude"].get<double>();
    if (params.contains("sample_rate"))
        sample_rate_ = params["sample_rate"].get<double>();
    if (params.contains("duration_sec"))
        duration_sec_ = params["duration_sec"].get<double>();
    if (params.contains("seed"))
        seed_ = params["seed"].get<uint32_t>();
}

size_t SourceBase::compute_block_size(size_t max_samples) const {
    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
        if (samples_produced_ >= total_samples)
            return 0;
        total_available = total_samples - samples_produced_;
    }
    return std::min(max_samples, total_available);
}

void SourceBase::fill_common_metadata(WaveformMetadata& meta) const {
    meta.sample_rate = sample_rate_;
    meta.duration_sec = duration_sec_;
    meta.repeats = !duration_sec_.has_value();
}

void SourceBase::reset_common() {
    samples_produced_ = 0;
}

void SourceBase::validate_positive(double value, const char* name) {
    if (value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be positive, got " + std::to_string(value));
    }
}

void SourceBase::validate_non_negative(double value, const char* name) {
    if (value < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be non-negative, got " + std::to_string(value));
    }
}

} // namespace archerfish::dsp
