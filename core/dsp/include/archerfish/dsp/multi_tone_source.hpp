#pragma once

#include <complex>
#include <cstddef>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

struct ToneSpec {
    double frequency_hz{0.0};
    double amplitude{0.2};
};

class MultiToneSource : public ISource {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    std::vector<ToneSpec> tones_;
    double sample_rate_{1e6};
    std::optional<double> duration_sec_;
    size_t samples_generated_{0};
};

} // namespace archerfish::dsp
