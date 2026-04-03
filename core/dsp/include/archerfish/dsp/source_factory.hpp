#pragma once

#include <memory>
#include <string>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

/// Create a source instance from a waveform type string.
/// Returns nullptr if the type is unknown.
[[nodiscard]] std::unique_ptr<ISource> create_source(const std::string& type);

} // namespace archerfish::dsp
