#pragma once

#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace archerfish::impairments::detail {

inline void require_finite(double value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

inline void require_positive_finite(double value, const char* name) {
    require_finite(value, name);
    if (value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be > 0");
    }
}

inline void require_nonnegative_finite(double value, const char* name) {
    require_finite(value, name);
    if (value < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be >= 0");
    }
}

inline void require_probability(double value, const char* name) {
    require_finite(value, name);
    if (value < 0.0 || value > 1.0) {
        throw std::invalid_argument(std::string(name) + " must be in [0, 1]");
    }
}

inline void require_apply_buffer(const std::complex<float>* data, size_t count, const char* name) {
    if (count == 0) {
        return;
    }
    if (data == nullptr) {
        throw std::invalid_argument(std::string(name) + " apply buffer must not be null");
    }
}

} // namespace archerfish::impairments::detail
