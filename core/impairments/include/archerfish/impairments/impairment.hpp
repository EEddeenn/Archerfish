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
    virtual bool enabled() const = 0;
    virtual void set_enabled(bool v) = 0;
};

} // namespace archerfish::impairments
