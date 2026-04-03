#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::impairments {

class ImpairmentChain {
public:
    void add(std::unique_ptr<IImpairment> impairment);
    void apply(std::complex<float>* data, size_t count);
    size_t size() const;
    IImpairment& at(size_t index);
    void set_enabled(size_t index, bool enabled);
    void clear();

private:
    std::vector<std::unique_ptr<IImpairment>> chain_;
};

} // namespace archerfish::impairments
