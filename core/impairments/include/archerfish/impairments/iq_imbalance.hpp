#pragma once

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class IqImbalanceImpairment : public IImpairment {
public:
    IqImbalanceImpairment(double gain_imbalance_db = 0.0,
                          double phase_imbalance_rad = 0.0);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override;
    bool enabled() const override;
    void set_enabled(bool v) override;

private:
    double gain_imbalance_db_;
    double phase_imbalance_rad_;
    bool enabled_{true};
};

} // namespace archerfish::impairments
