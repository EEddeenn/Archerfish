#include "archerfish/impairments/dc_offset.hpp"

#include <complex>
#include <cstddef>

#include "validation.hpp"

namespace archerfish::impairments {

DcOffsetImpairment::DcOffsetImpairment(double dc_i, double dc_q)
    : dc_i_(dc_i),
      dc_q_(dc_q) {
    detail::require_finite(dc_i_, "DC I offset");
    detail::require_finite(dc_q_, "DC Q offset");
}

void DcOffsetImpairment::apply(std::complex<float>* data, size_t count) {
    detail::require_apply_buffer(data, count, "DcOffsetImpairment");
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

} // namespace archerfish::impairments
