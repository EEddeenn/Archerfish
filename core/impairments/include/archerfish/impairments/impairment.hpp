#pragma once

#include <complex>
#include <cstddef>
#include <string>

namespace archerfish::impairments {

class IImpairment {
public:
    virtual ~IImpairment() = default;
    virtual void apply(std::complex<float>* data, size_t count) = 0;
    virtual std::string name() const = 0;

    bool enabled() const { return enabled_; }
    void set_enabled(bool v) { enabled_ = v; }

protected:
    bool enabled_{true};
};

} // namespace archerfish::impairments
