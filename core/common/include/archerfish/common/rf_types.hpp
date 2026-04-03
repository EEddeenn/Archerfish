#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "archerfish/common/error.hpp"

namespace archerfish::common {

struct TimeSpec {
    int64_t seconds{0};
    double fractional_ns{0.0};

    [[nodiscard]] double to_seconds() const;

    static TimeSpec from_seconds(double sec);

    [[nodiscard]] bool operator<(const TimeSpec& other) const;
    [[nodiscard]] bool operator>=(const TimeSpec& other) const;
};

using Duration = double;
using ChannelId = uint32_t;
using DeviceId = std::string;

} // namespace archerfish::common
