#include "archerfish/impairments/phase_noise.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

#include "archerfish/common/constants.hpp"
#include "validation.hpp"

namespace archerfish::impairments {

PhaseNoiseImpairment::PhaseNoiseImpairment(double bandwidth_hz, double magnitude_rad,
                                           double sample_rate, const std::string& psd_shape)
    : bandwidth_hz_(bandwidth_hz),
      magnitude_rad_(magnitude_rad),
      sample_rate_(sample_rate),
      rng_(std::random_device{}()),
      dist_(0.0, 1.0) {
    detail::require_nonnegative_finite(bandwidth_hz_, "Phase noise bandwidth");
    detail::require_nonnegative_finite(magnitude_rad_, "Phase noise magnitude");
    detail::require_positive_finite(sample_rate_, "Phase noise sample rate");
    alpha_ = std::exp(-2.0 * archerfish::constants::kPi * bandwidth_hz_ / sample_rate_);
    (void)psd_shape; // reserved for future PSD shaping
}

void PhaseNoiseImpairment::apply(std::complex<float>* data, size_t count) {
    detail::require_apply_buffer(data, count, "PhaseNoiseImpairment");
    if (!enabled_) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        phase_offset_ = alpha_ * phase_offset_ + std::sqrt(1.0 - alpha_ * alpha_) * dist_(rng_) * magnitude_rad_;
        float cos_p = static_cast<float>(std::cos(phase_offset_));
        float sin_p = static_cast<float>(std::sin(phase_offset_));
        float re = data[i].real();
        float im = data[i].imag();
        data[i] = std::complex<float>(re * cos_p - im * sin_p,
                                       re * sin_p + im * cos_p);
    }
}

} // namespace archerfish::impairments
