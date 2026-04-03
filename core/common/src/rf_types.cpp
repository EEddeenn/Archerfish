#include "archerfish/common/rf_types.hpp"

#include <cmath>
#include <fmt/format.h>

namespace archerfish::common {

double TimeSpec::to_seconds() const {
    return static_cast<double>(seconds) + fractional_ns * 1e-9;
}

TimeSpec TimeSpec::from_seconds(double sec) {
    double int_part;
    double frac = std::modf(sec, &int_part);
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
