#include "archerfish/scenario/scenario.hpp"

#include <cmath>

#include <fmt/format.h>

namespace archerfish::scenario {

using common::Error;
using common::ErrorCategory;
using common::ErrorList;

ErrorList RfSettings::validate() const {
    ErrorList errors;
    if (!std::isfinite(freq_hz) || freq_hz <= 0.0) {
        errors.push_back({ErrorCategory::Validation,
                          "E_RF_INVALID_FREQ",
                          fmt::format("Frequency must be positive, got {} Hz", freq_hz)});
    }
    if (!std::isfinite(rate_sps) || rate_sps <= 0.0) {
        errors.push_back({ErrorCategory::Validation,
                          "E_RF_INVALID_RATE",
                          fmt::format("Sample rate must be positive, got {} sps", rate_sps)});
    }
    if (!std::isfinite(gain_db) || gain_db < -100.0 || gain_db > 100.0) {
        errors.push_back({ErrorCategory::Validation,
                          "E_RF_INVALID_GAIN",
                          fmt::format("Gain should be in [-100, 100] dB, got {} dB", gain_db)});
    }
    if (bandwidth_hz.has_value() && (!std::isfinite(*bandwidth_hz) || *bandwidth_hz <= 0.0)) {
        errors.push_back({ErrorCategory::Validation,
                          "E_RF_INVALID_BW",
                          fmt::format("Bandwidth must be positive, got {} Hz", *bandwidth_hz)});
    }
    return errors;
}

} // namespace archerfish::scenario
