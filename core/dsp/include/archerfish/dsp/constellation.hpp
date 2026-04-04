#pragma once

#include <complex>
#include <vector>

#include "archerfish/dsp/modulation_type.hpp"

namespace archerfish::dsp {

/// Build a normalized constellation for the given modulation type.
/// Returns unit-average-power constellation points.
[[nodiscard]] std::vector<std::complex<float>> build_constellation(ModulationType mod);

} // namespace archerfish::dsp
