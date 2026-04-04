#pragma once

#include <cstddef>
#include <string>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class PaNonlinearityImpairment : public IImpairment {
public:
    // model: "rapp" or "saleh"
    // For Rapp: saturation=1.0, smoothness=2.0
    // For Saleh: a_params={a0,a1}, b_params={b0,b1} with defaults
    PaNonlinearityImpairment(const std::string& model, double saturation = 1.0,
                             double smoothness = 2.0, double phase_shift = 0.0);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override { return "pa_nonlinearity"; }

private:
    std::string model_;
    double saturation_;
    double smoothness_;
    double phase_shift_;
    // Saleh model parameters (defaults from literature)
    double saleh_a0_{2.1587}, saleh_a1_{1.1517};
    double saleh_b0_{4.0033}, saleh_b1_{9.1040};

    // AM/AM functions
    double rapp_am_am(double input_amplitude) const;
    double saleh_am_am(double input_amplitude) const;
    // AM/PM functions (returns phase shift in radians)
    double rapp_am_pm(double input_amplitude) const;
    double saleh_am_pm(double input_amplitude) const;
};

} // namespace archerfish::impairments
