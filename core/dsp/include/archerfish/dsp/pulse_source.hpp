#pragma once

#include <complex>
#include <cstddef>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

class PulseSource : public ISource {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    double amplitude_{0.2};
    double frequency_hz_{0.0};
    double sample_rate_{1e6};
    double pulse_width_sec_{1e-6};
    double pri_sec_{10e-6};
    std::string mode_{"train"};
    std::optional<double> duration_sec_;

    size_t pw_samples_{0};
    size_t pri_samples_{0};

    float phase_{0.0f};
    size_t samples_generated_{0};
    bool pulse_done_{false};
};

} // namespace archerfish::dsp
