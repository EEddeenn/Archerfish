#include "archerfish/impairments/phase_offset.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

namespace archerfish::impairments {

PhaseOffsetImpairment::PhaseOffsetImpairment(double phase_rad)
    : phase_rad_(phase_rad) {}

void PhaseOffsetImpairment::apply(std::complex<float>* data, size_t count) {
    if (!enabled_) {
        return;
    }
    float cos_p = static_cast<float>(std::cos(phase_rad_));
    float sin_p = static_cast<float>(std::sin(phase_rad_));
    std::complex<float> rot(cos_p, sin_p);
    for (size_t i = 0; i < count; ++i) {
        data[i] *= rot;
    }
}

std::string PhaseOffsetImpairment::name() const {
    return "phase_offset";
}

} // namespace archerfish::impairments
