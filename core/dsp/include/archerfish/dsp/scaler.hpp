#pragma once

#include <complex>
#include <cstddef>

namespace archerfish::dsp {

void scale(std::complex<float>* buffer, size_t n, std::complex<float> factor);
void scale(std::complex<float>* buffer, size_t n, float factor);

} // namespace archerfish::dsp
