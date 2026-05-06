#include "archerfish/dsp/source_base.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace archerfish::dsp {

namespace {

uint32_t parse_seed(const nlohmann::json& value) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        throw std::invalid_argument("seed must be an unsigned 32-bit integer");
    }

    if (value.is_number_integer()) {
        const auto parsed = value.get<std::int64_t>();
        if (parsed < 0) {
            throw std::invalid_argument("seed must be non-negative");
        }
        if (parsed > static_cast<std::int64_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::out_of_range("seed exceeds uint32 range");
        }
        return static_cast<uint32_t>(parsed);
    }

    const auto parsed = value.get<std::uint64_t>();
    if (parsed > std::numeric_limits<uint32_t>::max()) {
        throw std::out_of_range("seed exceeds uint32 range");
    }
    return static_cast<uint32_t>(parsed);
}

} // namespace

void SourceBase::configure_common(const nlohmann::json& params) {
    if (!params.is_object()) {
        throw std::invalid_argument("source parameters must be an object");
    }
    double next_amplitude = amplitude_;
    double next_sample_rate = sample_rate_;
    std::optional<double> next_duration_sec = duration_sec_;
    uint32_t next_seed = seed_;

    if (params.contains("amplitude")) {
        next_amplitude = number_param(params, "amplitude");
        validate_non_negative(next_amplitude, "amplitude");
        if (next_amplitude > static_cast<double>(std::numeric_limits<float>::max())) {
            throw std::out_of_range("amplitude exceeds float range");
        }
    }
    if (params.contains("sample_rate")) {
        next_sample_rate = number_param(params, "sample_rate");
        validate_positive(next_sample_rate, "sample_rate");
    }
    if (params.contains("duration_sec")) {
        next_duration_sec = number_param(params, "duration_sec");
        validate_non_negative(*next_duration_sec, "duration_sec");
        (void)checked_sample_count(next_sample_rate, *next_duration_sec);
    }
    if (params.contains("seed"))
        next_seed = parse_seed(params["seed"]);

    amplitude_ = next_amplitude;
    sample_rate_ = next_sample_rate;
    duration_sec_ = next_duration_sec;
    seed_ = next_seed;
    reset_common();
}

size_t SourceBase::compute_block_size(size_t max_samples) const {
    size_t total_available = std::numeric_limits<size_t>::max();
    if (duration_sec_.has_value()) {
        size_t total_samples = checked_sample_count(sample_rate_, duration_sec_.value());
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
    if (!std::isfinite(value) || value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be positive, got " + std::to_string(value));
    }
}

void SourceBase::validate_non_negative(double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be non-negative, got " + std::to_string(value));
    }
}

size_t SourceBase::checked_sample_count(double sample_rate, double duration_sec) {
    validate_positive(sample_rate, "sample_rate");
    validate_non_negative(duration_sec, "duration_sec");

    const long double total = static_cast<long double>(sample_rate) * static_cast<long double>(duration_sec);
    if (!std::isfinite(total)) {
        throw std::overflow_error("sample count is too large");
    }

    const long double rounded = std::round(total);
    if (rounded < 0.0L ||
        rounded > static_cast<long double>(std::numeric_limits<size_t>::max())) {
        throw std::overflow_error("sample count is too large");
    }

    return static_cast<size_t>(rounded);
}

size_t SourceBase::checked_positive_rounded_count(double value, const char* name) {
    if (std::isinf(value)) {
        throw std::overflow_error(std::string(name) + " is too large");
    }
    if (std::isnan(value) || value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and positive");
    }

    const long double rounded = std::round(static_cast<long double>(value));
    if (rounded > static_cast<long double>(std::numeric_limits<size_t>::max())) {
        throw std::overflow_error(std::string(name) + " is too large");
    }
    if (rounded < 1.0L) {
        throw std::invalid_argument(std::string(name) + " rounds to zero");
    }

    return static_cast<size_t>(rounded);
}

double SourceBase::number_param(const nlohmann::json& params, const char* key) {
    const auto& value = params.at(key);
    if (!value.is_number()) {
        throw std::invalid_argument(std::string(key) + " must be a number");
    }
    return value.get<double>();
}

std::string SourceBase::string_param(const nlohmann::json& params, const char* key) {
    const auto& value = params.at(key);
    if (!value.is_string()) {
        throw std::invalid_argument(std::string(key) + " must be a string");
    }
    return value.get<std::string>();
}

bool SourceBase::bool_param(const nlohmann::json& params, const char* key) {
    const auto& value = params.at(key);
    if (!value.is_boolean()) {
        throw std::invalid_argument(std::string(key) + " must be a boolean");
    }
    return value.get<bool>();
}

int SourceBase::int_param(const nlohmann::json& params, const char* key) {
    const auto& value = params.at(key);
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        throw std::invalid_argument(std::string(key) + " must be an integer");
    }
    if (value.is_number_integer()) {
        const auto parsed = value.get<std::int64_t>();
        if (parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) {
            throw std::out_of_range(std::string(key) + " exceeds int range");
        }
        return static_cast<int>(parsed);
    }
    const auto parsed = value.get<std::uint64_t>();
    if (parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::out_of_range(std::string(key) + " exceeds int range");
    }
    return static_cast<int>(parsed);
}

} // namespace archerfish::dsp
