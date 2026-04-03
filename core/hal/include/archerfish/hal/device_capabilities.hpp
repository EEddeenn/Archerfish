#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace archerfish::hal {

/// A closed interval [min_val, max_val].
struct Range {
    double min_val{0.0};
    double max_val{0.0};

    [[nodiscard]] bool contains(double val) const {
        return val >= min_val && val <= max_val;
    }
};

/// Static capabilities reported by a device implementation.
struct DeviceCapabilities {
    uint32_t num_channels{1};
    Range freq_range{0.0, 6e9};
    Range rate_range{1e6, 56e6};
    Range gain_range{0.0, 90.0};
    Range bandwidth_range{1e6, 56e6};
    std::vector<std::string> supported_clock_sources{"internal"};
    std::vector<std::string> supported_time_sources{"none"};
    bool supports_replay{false};
};

} // namespace archerfish::hal
