#pragma once

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class PhaseOffsetImpairment : public IImpairment {
public:
    explicit PhaseOffsetImpairment(double phase_rad = 0.0);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override;
    bool enabled() const override;
    void set_enabled(bool v) override;

private:
    double phase_rad_;
    bool enabled_{true};
};

} // namespace archerfish::impairments
