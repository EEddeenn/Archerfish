#pragma once

#include <complex>
#include <cstddef>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source_base.hpp"

namespace archerfish::dsp {

class ChirpSource : public SourceBase {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    double f0_hz_{0.0};
    double f1_hz_{0.0};
};

} // namespace archerfish::dsp
