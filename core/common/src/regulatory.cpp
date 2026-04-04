#include "archerfish/common/regulatory.hpp"

#include <cmath>
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
    if (bandwidth_hz < 0.0) bandwidth_hz = 0.0;
    auto bands = get_default_restricted_bands();
    double half_bw = bandwidth_hz / 2.0;

    for (const auto& band : bands) {
        bool overlap = freq_hz + half_bw >= band.low_hz && freq_hz - half_bw <= band.high_hz;
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
