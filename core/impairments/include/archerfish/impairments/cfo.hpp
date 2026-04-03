#pragma once

#include <cstddef>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class CfoImpairment : public IImpairment {
public:
    CfoImpairment(double cfo_hz = 0.0, double sample_rate = 1.0);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override;
    bool enabled() const override;
    void set_enabled(bool v) override;

private:
    double cfo_hz_;
    double sample_rate_;
    bool enabled_{true};
    size_t sample_counter_{0};
};

} // namespace archerfish::impairments
