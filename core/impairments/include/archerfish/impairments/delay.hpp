#pragma once

#include <cstddef>
#include <deque>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class DelayImpairment : public IImpairment {
public:
    DelayImpairment(double delay_sec = 0.0, double sample_rate = 1.0);

    void apply(std::complex<float>* data, size_t count) override;
    std::string name() const override;
    bool enabled() const override;
    void set_enabled(bool v) override;

    void reset();

private:
    size_t delay_samples_;
    bool enabled_{true};
    std::deque<std::complex<float>> buffer_;
};

} // namespace archerfish::impairments
