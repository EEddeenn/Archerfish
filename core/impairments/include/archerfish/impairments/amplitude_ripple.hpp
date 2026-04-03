#pragma once

#include <cstddef>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class AmplitudeRippleImpairment : public IImpairment {
public:
    AmplitudeRippleImpairment(double ripple_depth = 0.0, double ripple_freq_hz = 0.0, double sample_rate = 1.0);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override;

private:
    double ripple_depth_;
    double ripple_freq_hz_;
    double sample_rate_;
    size_t sample_counter_{0};
};

} // namespace archerfish::impairments
