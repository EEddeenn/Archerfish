#include "archerfish/impairments/pa_nonlinearity.hpp"

#include <cmath>
#include <complex>
#include <stdexcept>

#include "validation.hpp"

namespace archerfish::impairments {

PaNonlinearityImpairment::PaNonlinearityImpairment(const std::string& model,
                                                   double saturation,
                                                   double smoothness,
                                                   double phase_shift)
    : model_(model),
      saturation_(saturation),
      smoothness_(smoothness),
      phase_shift_(phase_shift) {
    if (model_ != "rapp" && model_ != "saleh") {
        throw std::invalid_argument("PA nonlinearity model must be 'rapp' or 'saleh'");
    }
    detail::require_positive_finite(saturation_, "PA saturation");
    detail::require_positive_finite(smoothness_, "PA smoothness");
    detail::require_finite(phase_shift_, "PA phase shift");
    if (saturation_ <= 0.0) {
        throw std::invalid_argument("PA saturation must be positive");
    }
    if (smoothness_ <= 0.0) {
        throw std::invalid_argument("PA smoothness must be positive");
    }
}

// Rapp AM/AM: g(A) = A / (1 + (A/A_sat)^(2*p))^(1/(2*p))
double PaNonlinearityImpairment::rapp_am_am(double input_amplitude) const {
    double ratio = input_amplitude / saturation_;
    double p = smoothness_;
    double denom = std::pow(1.0 + std::pow(ratio, 2.0 * p), 1.0 / (2.0 * p));
    return input_amplitude / denom;
}

// Saleh AM/AM: g(A) = a0 * A / (1 + a1 * A^2)
double PaNonlinearityImpairment::saleh_am_am(double input_amplitude) const {
    return saleh_a0_ * input_amplitude / (1.0 + saleh_a1_ * input_amplitude * input_amplitude);
}

// Rapp AM/PM: linear phase shift proportional to amplitude
double PaNonlinearityImpairment::rapp_am_pm(double input_amplitude) const {
    return phase_shift_ * input_amplitude / saturation_;
}

// Saleh AM/PM: Phi(A) = b0 * A^2 / (1 + b1 * A^2)
double PaNonlinearityImpairment::saleh_am_pm(double input_amplitude) const {
    double a2 = input_amplitude * input_amplitude;
    return saleh_b0_ * a2 / (1.0 + saleh_b1_ * a2);
}

void PaNonlinearityImpairment::apply(std::complex<float>* data, size_t count) {
    detail::require_apply_buffer(data, count, "PaNonlinearityImpairment");
    if (!enabled_) return;

    for (size_t i = 0; i < count; ++i) {
        double amp = std::abs(data[i]);
        if (amp == 0.0) continue;

        double a_out;
        double phi;

        if (model_ == "rapp") {
            a_out = rapp_am_am(amp);
            phi = rapp_am_pm(amp);
        } else {
            a_out = saleh_am_am(amp);
            phi = saleh_am_pm(amp);
        }

        double theta = std::arg(data[i]);
        double new_phase = theta + phi;
        data[i] = std::complex<float>(
            static_cast<float>(a_out * std::cos(new_phase)),
            static_cast<float>(a_out * std::sin(new_phase)));
    }
}

} // namespace archerfish::impairments
