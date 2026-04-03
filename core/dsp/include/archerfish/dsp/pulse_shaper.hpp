#pragma once

#include <cstddef>
#include <vector>

namespace archerfish::dsp {

struct RrcFilterDesign {
    double alpha{0.35};
    size_t span_symbols{6};
    size_t samples_per_symbol{4};

    [[nodiscard]] std::vector<float> design() const;
    [[nodiscard]] size_t num_taps() const;
};

} // namespace archerfish::dsp
