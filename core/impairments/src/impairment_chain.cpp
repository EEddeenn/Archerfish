#include "archerfish/impairments/impairment_chain.hpp"

#include <complex>
#include <cstddef>
#include <stdexcept>

namespace archerfish::impairments {

void ImpairmentChain::add(std::unique_ptr<IImpairment> impairment) {
    chain_.push_back(std::move(impairment));
}

void ImpairmentChain::apply(std::complex<float>* data, size_t count) {
    for (auto& impairment : chain_) {
        impairment->apply(data, count);
    }
}

size_t ImpairmentChain::size() const {
    return chain_.size();
}

IImpairment& ImpairmentChain::at(size_t index) {
    if (index >= chain_.size()) {
        throw std::out_of_range("ImpairmentChain index out of range");
    }
    return *chain_[index];
}

void ImpairmentChain::set_enabled(size_t index, bool enabled) {
    at(index).set_enabled(enabled);
}

void ImpairmentChain::clear() {
    chain_.clear();
}

} // namespace archerfish::impairments
