#include "archerfish/impairments/delay.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

namespace archerfish::impairments {

DelayImpairment::DelayImpairment(double delay_sec, double sample_rate)
    : delay_samples_(static_cast<size_t>(std::round(delay_sec * sample_rate))) {
    buffer_.assign(delay_samples_, std::complex<float>(0.0f, 0.0f));
}

void DelayImpairment::apply(std::complex<float>* data, size_t count) {
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
