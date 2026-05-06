#include "archerfish/impairments/cfo.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

#include "archerfish/common/constants.hpp"
#include "validation.hpp"

namespace archerfish::impairments {

CfoImpairment::CfoImpairment(double cfo_hz, double sample_rate)
    : cfo_hz_(cfo_hz),
      sample_rate_(sample_rate) {
    detail::require_finite(cfo_hz_, "CFO frequency");
    detail::require_positive_finite(sample_rate_, "CFO sample rate");
}

void CfoImpairment::apply(std::complex<float>* data, size_t count) {
    detail::require_apply_buffer(data, count, "CfoImpairment");
    if (!enabled_) {
        return;
    }
    const double two_pi = archerfish::constants::kTwoPi;
    double phase_inc = two_pi * cfo_hz_ / sample_rate_;
    for (size_t i = 0; i < count; ++i) {
        double phase = phase_inc * static_cast<double>(sample_counter_);
        float cos_p = static_cast<float>(std::cos(phase));
        float sin_p = static_cast<float>(std::sin(phase));
        std::complex<float> rot(cos_p, sin_p);
        data[i] *= rot;
        ++sample_counter_;
    }
}

std::string CfoImpairment::name() const {
    return "cfo";
}

} // namespace archerfish::impairments
