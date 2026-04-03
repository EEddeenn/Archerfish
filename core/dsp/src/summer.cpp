#include "archerfish/dsp/summer.hpp"

#include <cmath>

#include <spdlog/spdlog.h>

namespace archerfish::dsp {

void summer(const std::complex<float>* a, const std::complex<float>* b,
            std::complex<float>* out, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        out[i] = a[i] + b[i];
    }
}

bool summer_with_headroom_check(const std::complex<float>* a,
                                const std::complex<float>* b,
                                std::complex<float>* out, size_t n) {
    bool exceeds = false;
    for (size_t i = 0; i < n; ++i) {
        out[i] = a[i] + b[i];
        float mag = std::abs(out[i]);
        if (mag > 1.0f) {
            exceeds = true;
        }
    }
    if (exceeds) {
        spdlog::warn("Summer: output exceeds 1.0 amplitude headroom");
    }
    return exceeds;
}

} // namespace archerfish::dsp
