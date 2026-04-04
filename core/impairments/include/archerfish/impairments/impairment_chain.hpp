#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "archerfish/impairments/impairment.hpp"

namespace archerfish::scenario {
struct ImpairmentSettings;
} // namespace archerfish::scenario

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

/// Factory: build an impairment chain from scenario settings.
/// Returns nullptr when no impairments are configured.
[[nodiscard]] std::unique_ptr<ImpairmentChain> build_chain(
    const scenario::ImpairmentSettings& settings,
    double sample_rate);

} // namespace archerfish::impairments
