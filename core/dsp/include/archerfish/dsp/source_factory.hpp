#pragma once

#include <memory>
#include <string>

#include "archerfish/dsp/source.hpp"
#include "archerfish/dsp/waveform_type.hpp"

namespace archerfish::dsp {

[[nodiscard]] std::unique_ptr<ISource> create_source(WaveformType type);

[[nodiscard]] std::unique_ptr<ISource> create_source(const std::string& type);

} // namespace archerfish::dsp
