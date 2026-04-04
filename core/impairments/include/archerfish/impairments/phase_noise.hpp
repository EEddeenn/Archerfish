#pragma once

#include <cstddef>
#include <random>
#include <string>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class PhaseNoiseImpairment : public IImpairment {
public:
    PhaseNoiseImpairment(double bandwidth_hz, double magnitude_rad,
                         double sample_rate, const std::string& psd_shape = "1f");

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override { return "phase_noise"; }

private:
    double bandwidth_hz_;
    double magnitude_rad_;
    double sample_rate_;
    double phase_offset_{0.0};
    double alpha_{0.0}; // filter coefficient for bandwidth limiting
    std::mt19937 rng_;
    std::normal_distribution<double> dist_;
};

} // namespace archerfish::impairments
