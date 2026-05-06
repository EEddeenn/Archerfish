#include "archerfish/common/safety_profile.hpp"

#include <cmath>
#include <fmt/format.h>

namespace archerfish::common {

SafetyProfile get_lab_safe_profile() {
    return SafetyProfile{
        .name = "lab_safe",
        .max_gain_db = 20.0,
        .max_amplitude = 0.5,
        .min_freq_hz = 900e6,
        .max_freq_hz = 6e9,
    };
}

common::ErrorList check_safety_profile(
    const SafetyProfile& profile,
    double gain_db,
    double amplitude,
    double freq_hz) {
    common::ErrorList errors;

    if (!std::isfinite(profile.max_gain_db) || !std::isfinite(profile.max_amplitude) ||
        !std::isfinite(profile.min_freq_hz) || !std::isfinite(profile.max_freq_hz) ||
        profile.max_amplitude < 0.0 || profile.min_freq_hz <= 0.0 ||
        profile.max_freq_hz <= profile.min_freq_hz) {
        errors.push_back({common::ErrorCategory::Validation,
                          "V_SAFETY_INVALID_PROFILE",
                          "Invalid safety profile limits"});
        return errors;
    }

    if (!std::isfinite(gain_db) || !std::isfinite(amplitude) || !std::isfinite(freq_hz)) {
        errors.push_back({common::ErrorCategory::Validation,
                          "V_SAFETY_NONFINITE_INPUT",
                          "Non-finite value passed to safety check"});
        return errors;
    }
    if (amplitude < 0.0) {
        errors.push_back({common::ErrorCategory::Validation,
                          "V_SAFETY_INVALID_AMPLITUDE",
                          "Safety check amplitude must be >= 0"});
        return errors;
    }
    if (freq_hz <= 0.0) {
        errors.push_back({common::ErrorCategory::Validation,
                          "V_SAFETY_INVALID_FREQUENCY",
                          "Safety check frequency must be > 0"});
        return errors;
    }

    if (gain_db > profile.max_gain_db) {
        errors.push_back({
            common::ErrorCategory::QualityWarning,
            "W_SAFETY_GAIN_EXCEEDED",
            fmt::format("Gain {:.1f} dB exceeds safety profile '{}' limit {:.1f} dB",
                        gain_db, profile.name, profile.max_gain_db)
        });
    }

    if (amplitude > profile.max_amplitude) {
        errors.push_back({
            common::ErrorCategory::QualityWarning,
            "W_SAFETY_AMPLITUDE_EXCEEDED",
            fmt::format("Amplitude {:.3f} exceeds safety profile '{}' limit {:.3f}",
                        amplitude, profile.name, profile.max_amplitude)
        });
    }

    if (freq_hz < profile.min_freq_hz || freq_hz > profile.max_freq_hz) {
        errors.push_back({
            common::ErrorCategory::QualityWarning,
            "W_SAFETY_FREQ_OUT_OF_RANGE",
            fmt::format("Frequency {:.3f} MHz outside safety profile '{}' range [{:.3f}–{:.3f}] MHz",
                        freq_hz / 1e6, profile.name,
                        profile.min_freq_hz / 1e6, profile.max_freq_hz / 1e6)
        });
    }

    return errors;
}

} // namespace archerfish::common
