#pragma once

#include <complex>
#include <cstddef>
#include <optional>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

class CwSource : public ISource {
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
    std::optional<double> duration_sec_;
    size_t samples_generated_{0};
};

} // namespace archerfish::dsp
