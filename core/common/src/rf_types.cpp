#include "archerfish/common/rf_types.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

#include <fmt/format.h>

namespace archerfish::common {

double TimeSpec::to_seconds() const {
    return static_cast<double>(seconds) + fractional_ns * 1e-9;
}

TimeSpec TimeSpec::from_seconds(double sec) {
    if (!std::isfinite(sec)) {
        throw std::invalid_argument("TimeSpec seconds must be finite");
    }

    double int_part = std::floor(sec);
    if (int_part < static_cast<double>(std::numeric_limits<int64_t>::lowest()) ||
        int_part >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
        throw std::out_of_range("TimeSpec seconds out of int64 range");
    }

    double frac = sec - int_part;
    return TimeSpec{static_cast<int64_t>(int_part), frac * 1e9};
}

bool TimeSpec::operator<(const TimeSpec& other) const {
    if (seconds != other.seconds) return seconds < other.seconds;
    return fractional_ns < other.fractional_ns;
}

bool TimeSpec::operator>=(const TimeSpec& other) const {
    return !(*this < other);
}

} // namespace archerfish::common
