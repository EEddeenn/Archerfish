#pragma once

#include <cstdint>
#include <random>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class AwgnImpairment : public IImpairment {
public:
    explicit AwgnImpairment(double noise_power = 0.0, uint32_t seed = 12345);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override;

private:
    double noise_power_;
    std::mt19937 generator_;
};

} // namespace archerfish::impairments
