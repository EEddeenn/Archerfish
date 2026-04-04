#pragma once

#include <string>
#include <vector>
#include "archerfish/common/error.hpp"

namespace archerfish::common {

struct RestrictedBand {
    std::string name;
    double low_hz{0.0};
    double high_hz{0.0};
    std::string region;
};

[[nodiscard]] std::vector<RestrictedBand> get_default_restricted_bands();

[[nodiscard]] common::ErrorList check_regulatory(double freq_hz, double bandwidth_hz = 0.0);

} // namespace archerfish::common
