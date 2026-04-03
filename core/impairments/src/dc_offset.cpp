#include "archerfish/impairments/dc_offset.hpp"

#include <complex>
#include <cstddef>

namespace archerfish::impairments {

DcOffsetImpairment::DcOffsetImpairment(double dc_i, double dc_q)
    : dc_i_(dc_i),
      dc_q_(dc_q) {}

void DcOffsetImpairment::apply(std::complex<float>* data, size_t count) {
    if (!enabled_) {
        return;
    }
    std::complex<float> offset(static_cast<float>(dc_i_), static_cast<float>(dc_q_));
    for (size_t i = 0; i < count; ++i) {
        data[i] += offset;
    }
}

std::string DcOffsetImpairment::name() const {
    return "dc_offset";
}

bool DcOffsetImpairment::enabled() const {
    return enabled_;
}

void DcOffsetImpairment::set_enabled(bool v) {
    enabled_ = v;
}

} // namespace archerfish::impairments
