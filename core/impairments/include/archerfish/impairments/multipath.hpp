#pragma once

#include <cstddef>
#include <vector>
#include <complex>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class MultipathImpairment : public IImpairment {
public:
    MultipathImpairment(size_t delay_samples, float amplitude);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override { return "multipath"; }

private:
    size_t delay_samples_;
    float amplitude_;
    std::vector<std::complex<float>> history_;
    size_t history_offset_{0};
};

} // namespace archerfish::impairments
