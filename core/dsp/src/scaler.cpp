#include "archerfish/dsp/scaler.hpp"

#include <cmath>
#include <stdexcept>

namespace archerfish::dsp {

void scale(std::complex<float>* buffer, size_t n, std::complex<float> factor) {
    if (n == 0) return;
    if (buffer == nullptr) {
        throw std::invalid_argument("scale: buffer must not be null");
    }
    if (!std::isfinite(factor.real()) || !std::isfinite(factor.imag())) {
        throw std::invalid_argument("scale: factor must be finite");
    }
    for (size_t i = 0; i < n; ++i) {
        buffer[i] *= factor;
    }
}

void scale(std::complex<float>* buffer, size_t n, float factor) {
    if (n == 0) return;
    if (buffer == nullptr) {
        throw std::invalid_argument("scale: buffer must not be null");
    }
    if (!std::isfinite(factor)) {
        throw std::invalid_argument("scale: factor must be finite");
    }
    for (size_t i = 0; i < n; ++i) {
        buffer[i] *= factor;
    }
}

} // namespace archerfish::dsp
