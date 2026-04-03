#include "archerfish/impairments/burst_dropout.hpp"

#include <complex>
#include <cstddef>

namespace archerfish::impairments {

BurstDropoutImpairment::BurstDropoutImpairment(double dropout_rate, double mean_burst_len, uint32_t seed)
    : dropout_rate_(dropout_rate),
      mean_burst_len_(mean_burst_len),
      rng_(seed),
      burst_start_dist_(dropout_rate),
      burst_len_dist_(1, static_cast<size_t>(2.0 * mean_burst_len)) {}

void BurstDropoutImpairment::apply(std::complex<float>* data, size_t count) {
    if (!enabled_ || dropout_rate_ <= 0.0) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (in_burst_) {
            data[i] = std::complex<float>(0.0f, 0.0f);
            burst_remaining_--;
            if (burst_remaining_ == 0) {
                in_burst_ = false;
            }
        } else if (burst_start_dist_(rng_)) {
            in_burst_ = true;
            burst_remaining_ = burst_len_dist_(rng_);
            data[i] = std::complex<float>(0.0f, 0.0f);
            if (burst_remaining_ == 0) {
                in_burst_ = false;
            } else {
                burst_remaining_--;
            }
        }
    }
}

std::string BurstDropoutImpairment::name() const {
    return "burst_dropout";
}

bool BurstDropoutImpairment::enabled() const {
    return enabled_;
}

void BurstDropoutImpairment::set_enabled(bool v) {
    enabled_ = v;
}

} // namespace archerfish::impairments
