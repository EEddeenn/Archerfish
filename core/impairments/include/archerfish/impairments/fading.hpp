#pragma once

#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class FadingImpairment : public IImpairment {
public:
    FadingImpairment(double doppler_hz, double sample_rate,
                     const std::string& type = "rayleigh", double k_factor = 0.0);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override { return "fading"; }

private:
    double sample_rate_;
    double doppler_hz_;
    double k_factor_;  // Rician K-factor (0 = Rayleigh)
    size_t num_sinusoids_{8};
    std::vector<double> phases_i_;  // random phases for I component
    std::vector<double> phases_q_;  // random phases for Q component
    std::vector<double> freqs_;     // Doppler frequencies
    size_t sample_index_{0};
    std::mt19937 rng_;

    void init_sinusoids();
    std::complex<float> compute_gain(size_t sample) const;
};

}  // namespace archerfish::impairments
