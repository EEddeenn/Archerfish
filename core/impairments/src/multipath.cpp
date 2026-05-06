#include "archerfish/impairments/multipath.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>

#include "validation.hpp"

namespace archerfish::impairments {

MultipathImpairment::MultipathImpairment(size_t delay_samples, float amplitude)
    : delay_samples_(delay_samples), amplitude_(amplitude) {
    detail::require_finite(amplitude_, "Multipath amplitude");
    history_.assign(delay_samples_, std::complex<float>(0.0f, 0.0f));
}

void MultipathImpairment::apply(std::complex<float>* data, size_t count) {
    detail::require_apply_buffer(data, count, "MultipathImpairment");
    if (!enabled_ || delay_samples_ == 0 || amplitude_ == 0.0f) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        auto delayed = history_[history_offset_];
        history_[history_offset_] = data[i];
        history_offset_ = (history_offset_ + 1) % delay_samples_;
        data[i] = data[i] + amplitude_ * delayed;
    }
}

} // namespace archerfish::impairments
