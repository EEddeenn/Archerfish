#pragma once

#include <complex>
#include <cstddef>
#include <random>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source_base.hpp"

namespace archerfish::dsp {

class NoiseSource : public SourceBase {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    std::mt19937 rng_;
    std::normal_distribution<float> dist_;
};

} // namespace archerfish::dsp
