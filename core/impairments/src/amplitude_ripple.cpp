#include "archerfish/impairments/amplitude_ripple.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

#include "archerfish/common/constants.hpp"
#include "validation.hpp"

namespace archerfish::impairments {

AmplitudeRippleImpairment::AmplitudeRippleImpairment(double ripple_depth, double ripple_freq_hz, double sample_rate)
    : ripple_depth_(ripple_depth),
      ripple_freq_hz_(ripple_freq_hz),
      sample_rate_(sample_rate) {
    detail::require_finite(ripple_depth_, "Amplitude ripple depth");
    detail::require_finite(ripple_freq_hz_, "Amplitude ripple frequency");
    detail::require_positive_finite(sample_rate_, "Amplitude ripple sample rate");
}

void AmplitudeRippleImpairment::apply(std::complex<float>* data, size_t count) {
    detail::require_apply_buffer(data, count, "AmplitudeRippleImpairment");
    if (!enabled_ || ripple_depth_ == 0.0) {
        return;
    }
    const double phase_inc = archerfish::constants::kTwoPi * ripple_freq_hz_ / sample_rate_;
    for (size_t i = 0; i < count; ++i) {
        double phase = phase_inc * static_cast<double>(sample_counter_ + i);
        float ripple = 1.0f + static_cast<float>(ripple_depth_) * static_cast<float>(std::sin(phase));
        data[i] *= ripple;
    }
    sample_counter_ += count;
}

std::string AmplitudeRippleImpairment::name() const {
    return "amplitude_ripple";
}

} // namespace archerfish::impairments
