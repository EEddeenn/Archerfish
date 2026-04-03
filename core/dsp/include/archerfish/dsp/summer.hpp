#pragma once

#include <complex>
#include <cstddef>

namespace archerfish::dsp {

void summer(const std::complex<float>* a, const std::complex<float>* b,
            std::complex<float>* out, size_t n);

bool summer_with_headroom_check(const std::complex<float>* a,
                                const std::complex<float>* b,
                                std::complex<float>* out, size_t n);

} // namespace archerfish::dsp
