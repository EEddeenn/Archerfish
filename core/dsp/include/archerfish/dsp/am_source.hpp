#pragma once

#include <complex>
#include <cstddef>
#include <optional>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

class AmSource : public ISource {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    double amplitude_{0.2};
    double carrier_freq_hz_{0.0};
    double mod_freq_hz_{1000.0};
    double mod_depth_{0.5};
    double sample_rate_{1e6};
    std::optional<double> duration_sec_;

    float carrier_phase_{0.0f};
    size_t samples_produced_{0};
};

} // namespace archerfish::dsp
