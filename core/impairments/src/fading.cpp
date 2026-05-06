#include "archerfish/impairments/fading.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

#include "archerfish/common/constants.hpp"
#include "validation.hpp"

namespace archerfish::impairments {

FadingImpairment::FadingImpairment(double doppler_hz, double sample_rate,
                                   const std::string& type, double k_factor)
    : sample_rate_(sample_rate),
      doppler_hz_(doppler_hz),
      k_factor_(type == "rician" ? k_factor : 0.0),
      rng_(42) {
    detail::require_positive_finite(sample_rate_, "Fading sample rate");
    detail::require_nonnegative_finite(doppler_hz_, "Fading doppler");
    if (type != "rayleigh" && type != "rician") {
        throw std::invalid_argument("Fading type must be 'rayleigh' or 'rician'");
    }
    if (type == "rician") {
        detail::require_nonnegative_finite(k_factor, "Fading K-factor");
    }
    init_sinusoids();
}

void FadingImpairment::init_sinusoids() {
    phases_i_.resize(num_sinusoids_);
    phases_q_.resize(num_sinusoids_);
    freqs_.resize(num_sinusoids_);

    std::uniform_real_distribution<double> phase_dist(0.0, archerfish::constants::kTwoPi);

    for (size_t k = 0; k < num_sinusoids_; ++k) {
        freqs_[k] = doppler_hz_ * static_cast<double>(k + 1) / static_cast<double>(num_sinusoids_);
        phases_i_[k] = phase_dist(rng_);
        phases_q_[k] = phase_dist(rng_);
    }
}

std::complex<float> FadingImpairment::compute_gain(size_t sample) const {
    const double t = static_cast<double>(sample) / sample_rate_;
    const double inv_sqrt_n = 1.0 / std::sqrt(static_cast<double>(num_sinusoids_));

    double gain_re = 0.0;
    double gain_im = 0.0;

    for (size_t k = 0; k < num_sinusoids_; ++k) {
        double angle_i = archerfish::constants::kTwoPi * freqs_[k] * t + phases_i_[k];
        double angle_q = archerfish::constants::kTwoPi * freqs_[k] * t + phases_q_[k];
        gain_re += std::cos(angle_i);
        gain_im += std::sin(angle_q);
    }

    gain_re *= inv_sqrt_n;
    gain_im *= inv_sqrt_n;

    if (k_factor_ > 0.0) {
        // Proper Rician: scale scattered by sqrt(1/(K+1)), LOS along real axis
        double scattered_scale = 1.0 / std::sqrt(k_factor_ + 1.0);
        gain_re *= scattered_scale;
        gain_im *= scattered_scale;
        double los = std::sqrt(k_factor_ / (k_factor_ + 1.0));
        gain_re += los;  // LOS along real axis only
    }

    return std::complex<float>(static_cast<float>(gain_re), static_cast<float>(gain_im));
}

void FadingImpairment::apply(std::complex<float>* data, size_t count) {
    detail::require_apply_buffer(data, count, "FadingImpairment");
    if (!enabled_) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        data[i] *= compute_gain(sample_index_);
        ++sample_index_;
    }
}

}  // namespace archerfish::impairments
