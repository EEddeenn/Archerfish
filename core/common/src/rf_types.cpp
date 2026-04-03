#include "archerfish/common/rf_types.hpp"

#include <cmath>
#include <fmt/format.h>

namespace archerfish::common {

ErrorList RfConfig::validate() const {
    ErrorList errors;
    if (freq_hz <= 0.0) {
        errors.push_back({ErrorCategory::Validation,
                          "E_RF_INVALID_FREQ",
                          fmt::format("Frequency must be positive, got {} Hz", freq_hz)});
    }
    if (rate_sps <= 0.0) {
        errors.push_back({ErrorCategory::Validation,
                          "E_RF_INVALID_RATE",
                          fmt::format("Sample rate must be positive, got {} sps", rate_sps)});
    }
    if (gain_db < -100.0 || gain_db > 100.0) {
        errors.push_back({ErrorCategory::Validation,
                          "E_RF_INVALID_GAIN",
                          fmt::format("Gain should be in [-100, 100] dB, got {} dB", gain_db)});
    }
    if (bandwidth_hz.has_value() && bandwidth_hz.value() <= 0.0) {
        errors.push_back({ErrorCategory::Validation,
                          "E_RF_INVALID_BW",
                          fmt::format("Bandwidth must be positive, got {} Hz", bandwidth_hz.value())});
    }
    return errors;
}

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
