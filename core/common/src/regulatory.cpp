#include "archerfish/common/regulatory.hpp"

#include <cmath>
#include <limits>
#include <fmt/format.h>

namespace archerfish::common {

std::vector<RestrictedBand> get_default_restricted_bands() {
    return {
        {"GPS L1", 1575.42e6 - 10e6, 1575.42e6 + 10e6, "Global"},
        {"Aviation VHF", 108e6, 137e6, "Global"},
        {"Cellular Uplink (850)", 824e6, 849e6, "US"},
        {"Emergency 121.5", 121.5e6 - 0.5e6, 121.5e6 + 0.5e6, "Global"},
    };
}

common::ErrorList check_regulatory(double freq_hz, double bandwidth_hz) {
    common::ErrorList warnings;
    if (!std::isfinite(freq_hz) || freq_hz <= 0.0) {
        warnings.push_back({common::ErrorCategory::Validation,
                            "V_REGULATORY_INVALID_FREQUENCY",
                            "Regulatory check frequency must be finite and > 0"});
        return warnings;
    }
    if (!std::isfinite(bandwidth_hz) || bandwidth_hz < 0.0) {
        warnings.push_back({common::ErrorCategory::Validation,
                            "V_REGULATORY_INVALID_BANDWIDTH",
                            "Regulatory check bandwidth must be finite and >= 0"});
        return warnings;
    }
    auto bands = get_default_restricted_bands();
    double half_bw = bandwidth_hz / 2.0;
    if (!std::isfinite(half_bw) ||
        half_bw > std::numeric_limits<double>::max() - freq_hz) {
        warnings.push_back({common::ErrorCategory::Validation,
                            "V_REGULATORY_INVALID_BANDWIDTH",
                            "Regulatory check bandwidth is too large for frequency"});
        return warnings;
    }
    const double signal_low = freq_hz - half_bw;
    const double signal_high = freq_hz + half_bw;

    for (const auto& band : bands) {
        if (!std::isfinite(band.low_hz) || !std::isfinite(band.high_hz) ||
            band.low_hz > band.high_hz) {
            continue;
        }
        bool overlap = signal_high >= band.low_hz && signal_low <= band.high_hz;
        if (overlap) {
            warnings.push_back({
                common::ErrorCategory::QualityWarning,
                "W_REGULATED_BAND",
                fmt::format("Frequency {:.3f} MHz falls within regulated band '{}' ({:.3f}–{:.3f} MHz, {})",
                            freq_hz / 1e6, band.name, band.low_hz / 1e6, band.high_hz / 1e6, band.region)
            });
        }
    }
    return warnings;
}

} // namespace archerfish::common
