#include "archerfish/dsp/scaler.hpp"

namespace archerfish::dsp {

void scale(std::complex<float>* buffer, size_t n, std::complex<float> factor) {
    for (size_t i = 0; i < n; ++i) {
        buffer[i] *= factor;
    }
}

void scale(std::complex<float>* buffer, size_t n, float factor) {
    for (size_t i = 0; i < n; ++i) {
        buffer[i] *= factor;
    }
}

} // namespace archerfish::dsp
