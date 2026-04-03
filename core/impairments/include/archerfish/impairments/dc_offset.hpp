#pragma once

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class DcOffsetImpairment : public IImpairment {
public:
    DcOffsetImpairment(double dc_i = 0.0, double dc_q = 0.0);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override;
    bool enabled() const override;
    void set_enabled(bool v) override;

private:
    double dc_i_;
    double dc_q_;
    bool enabled_{true};
};

} // namespace archerfish::impairments
