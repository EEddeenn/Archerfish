#pragma once

#include <optional>
#include <string>
#include "archerfish/common/error.hpp"

namespace archerfish::common {

struct SafetyProfile {
    std::string name;
    double max_gain_db{20.0};
    double max_amplitude{0.5};
    double min_freq_hz{900e6};
    double max_freq_hz{6e9};
};

[[nodiscard]] SafetyProfile get_lab_safe_profile();

[[nodiscard]] common::ErrorList check_safety_profile(
    const SafetyProfile& profile,
    double gain_db,
    double amplitude,
    double freq_hz);

} // namespace archerfish::common
