#pragma once

#include <complex>
#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source_base.hpp"

namespace archerfish::dsp {

class PulseSource : public SourceBase {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    double frequency_hz_{0.0};
    double pulse_width_sec_{1e-6};
    double pri_sec_{10e-6};
    std::string mode_{"train"};

    size_t pw_samples_{0};
    size_t pri_samples_{0};

    float phase_{0.0f};
    bool pulse_done_{false};
};

} // namespace archerfish::dsp
