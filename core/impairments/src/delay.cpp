#include "archerfish/impairments/delay.hpp"

#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>

#include "validation.hpp"

namespace archerfish::impairments {

namespace {

constexpr size_t kMaxDelaySamples = 16'000'000;

} // namespace

DelayImpairment::DelayImpairment(double delay_sec, double sample_rate)
    : delay_samples_(0) {
    detail::require_nonnegative_finite(delay_sec, "Delay");
    detail::require_positive_finite(sample_rate, "Delay sample rate");
    const double delay_samples = std::round(delay_sec * sample_rate);
    if (!std::isfinite(delay_samples) ||
        delay_samples > static_cast<double>(std::numeric_limits<size_t>::max())) {
        throw std::invalid_argument("Delay sample count is too large");
    }
    delay_samples_ = static_cast<size_t>(delay_samples);
    if (delay_samples_ > kMaxDelaySamples) {
        throw std::invalid_argument("Delay sample count is too large");
    }
    buffer_.assign(delay_samples_, std::complex<float>(0.0f, 0.0f));
}

void DelayImpairment::apply(std::complex<float>* data, size_t count) {
    detail::require_apply_buffer(data, count, "DelayImpairment");
    if (!enabled_ || delay_samples_ == 0) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        buffer_.push_back(data[i]);
        data[i] = buffer_.front();
        buffer_.pop_front();
    }
}

std::string DelayImpairment::name() const {
    return "delay";
}

void DelayImpairment::reset() {
    buffer_.clear();
    buffer_.assign(delay_samples_, std::complex<float>(0.0f, 0.0f));
}

} // namespace archerfish::impairments
