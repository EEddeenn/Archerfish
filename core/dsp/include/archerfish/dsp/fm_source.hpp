#pragma once

#include <complex>
#include <cstddef>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source_base.hpp"

namespace archerfish::dsp {

class FmSource : public SourceBase {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    double carrier_freq_hz_{0.0};
    double mod_freq_hz_{1000.0};
    double deviation_hz_{5000.0};

    double phase_{0.0};
};

} // namespace archerfish::dsp
