#include "archerfish/impairments/amplitude_ripple.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

namespace archerfish::impairments {

AmplitudeRippleImpairment::AmplitudeRippleImpairment(double ripple_depth, double ripple_freq_hz, double sample_rate)
    : ripple_depth_(ripple_depth),
      ripple_freq_hz_(ripple_freq_hz),
      sample_rate_(sample_rate) {}

void AmplitudeRippleImpairment::apply(std::complex<float>* data, size_t count) {
    if (!enabled_ || ripple_depth_ == 0.0) {
        return;
    }
    float phase_inc = static_cast<float>(2.0 * M_PI * ripple_freq_hz_ / sample_rate_);
    for (size_t i = 0; i < count; ++i) {
        float ripple = 1.0f + static_cast<float>(ripple_depth_) * std::sin(phase_inc * static_cast<float>(sample_counter_ + i));
        data[i] *= ripple;
    }
    sample_counter_ += count;
}

std::string AmplitudeRippleImpairment::name() const {
    return "amplitude_ripple";
}

bool AmplitudeRippleImpairment::enabled() const {
    return enabled_;
}

void AmplitudeRippleImpairment::set_enabled(bool v) {
    enabled_ = v;
}

} // namespace archerfish::impairments
