#include "archerfish/impairments/burst_dropout.hpp"

#include <algorithm>
#include <complex>
#include <cstddef>
#include <limits>

#include "validation.hpp"

namespace archerfish::impairments {

BurstDropoutImpairment::BurstDropoutImpairment(double dropout_rate, double mean_burst_len, uint32_t seed)
    : dropout_rate_(dropout_rate),
      mean_burst_len_(mean_burst_len),
      rng_(seed),
      burst_start_dist_(0.0),
      burst_len_dist_(1, 1) {
    detail::require_probability(dropout_rate_, "Burst dropout rate");
    detail::require_positive_finite(mean_burst_len_, "Burst dropout mean burst length");
    if (mean_burst_len_ > static_cast<double>(std::numeric_limits<size_t>::max() / 2)) {
        throw std::invalid_argument("Burst dropout mean burst length is too large");
    }
    burst_start_dist_ = std::bernoulli_distribution(dropout_rate_);
    burst_len_dist_ = std::uniform_int_distribution<size_t>(
        1, std::max<size_t>(1, static_cast<size_t>(2.0 * mean_burst_len_)));
}

void BurstDropoutImpairment::apply(std::complex<float>* data, size_t count) {
    detail::require_apply_buffer(data, count, "BurstDropoutImpairment");
    if (!enabled_ || dropout_rate_ <= 0.0) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!in_burst_ && burst_start_dist_(rng_)) {
            in_burst_ = true;
            burst_remaining_ = burst_len_dist_(rng_);
        }
        if (in_burst_) {
            data[i] = std::complex<float>(0.0f, 0.0f);
            --burst_remaining_;
            if (burst_remaining_ == 0) {
                in_burst_ = false;
            }
        }
    }
}

std::string BurstDropoutImpairment::name() const {
    return "burst_dropout";
}

} // namespace archerfish::impairments
