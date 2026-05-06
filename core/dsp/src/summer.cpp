#include "archerfish/dsp/summer.hpp"

#include <cmath>
#include <stdexcept>

#include <spdlog/spdlog.h>

namespace archerfish::dsp {

namespace {

void validate_buffers(const std::complex<float>* a, const std::complex<float>* b,
                      const std::complex<float>* out, size_t n) {
    if (n == 0) return;
    if (a == nullptr || b == nullptr || out == nullptr) {
        throw std::invalid_argument("summer: buffers must not be null");
    }
}

} // namespace

void summer(const std::complex<float>* a, const std::complex<float>* b,
            std::complex<float>* out, size_t n) {
    validate_buffers(a, b, out, n);
    for (size_t i = 0; i < n; ++i) {
        out[i] = a[i] + b[i];
    }
}

bool summer_with_headroom_check(const std::complex<float>* a,
                                const std::complex<float>* b,
                                std::complex<float>* out, size_t n) {
    validate_buffers(a, b, out, n);
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
