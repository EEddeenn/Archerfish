#include "archerfish/impairments/iq_imbalance.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

namespace archerfish::impairments {

IqImbalanceImpairment::IqImbalanceImpairment(double gain_imbalance_db,
                                             double phase_imbalance_rad)
    : gain_imbalance_db_(gain_imbalance_db),
      phase_imbalance_rad_(phase_imbalance_rad) {}

void IqImbalanceImpairment::apply(std::complex<float>* data, size_t count) {
    if (!enabled_) {
        return;
    }
    double a = std::pow(10.0, gain_imbalance_db_ / 20.0);
    double g = (a - 1.0) / (a + 1.0);
    double p = phase_imbalance_rad_;

    float one_plus_g = static_cast<float>(1.0 + g);
    float one_minus_g = static_cast<float>(1.0 - g);
    float sin_p = static_cast<float>(std::sin(p));
    float cos_p = static_cast<float>(std::cos(p));

    for (size_t i = 0; i < count; ++i) {
        float I = data[i].real();
        float Q = data[i].imag();
        float I_prime = one_plus_g * I;
        float Q_prime = one_minus_g * (sin_p * I + cos_p * Q);
        data[i] = std::complex<float>(I_prime, Q_prime);
    }
}

std::string IqImbalanceImpairment::name() const {
    return "iq_imbalance";
}

} // namespace archerfish::impairments
