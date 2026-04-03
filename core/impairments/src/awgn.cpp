#include "archerfish/impairments/awgn.hpp"

#include <cmath>
#include <complex>
#include <cstddef>

namespace archerfish::impairments {

AwgnImpairment::AwgnImpairment(double noise_power, uint32_t seed)
    : noise_power_(noise_power),
      generator_(seed) {}

void AwgnImpairment::apply(std::complex<float>* data, size_t count) {
    if (!enabled_ || noise_power_ <= 0.0) {
        return;
    }
    float sigma = static_cast<float>(std::sqrt(noise_power_ / 2.0));
    std::normal_distribution<float> noise(0.0f, sigma);
    for (size_t i = 0; i < count; ++i) {
        float ni = noise(generator_);
        float nq = noise(generator_);
        data[i] += std::complex<float>(ni, nq);
    }
}

std::string AwgnImpairment::name() const {
    return "awgn";
}

} // namespace archerfish::impairments
