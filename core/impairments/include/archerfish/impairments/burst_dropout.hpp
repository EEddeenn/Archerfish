#pragma once

#include <cstddef>
#include <cstdint>
#include <random>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class BurstDropoutImpairment : public IImpairment {
public:
    BurstDropoutImpairment(double dropout_rate = 0.0, double mean_burst_len = 10.0, uint32_t seed = 42);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override;

private:
    double dropout_rate_;
    [[maybe_unused]] double mean_burst_len_;
    std::mt19937 rng_;
    std::bernoulli_distribution burst_start_dist_;
    std::uniform_int_distribution<size_t> burst_len_dist_;
    bool in_burst_{false};
    size_t burst_remaining_{0};
};

} // namespace archerfish::impairments
