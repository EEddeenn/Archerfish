#pragma once

#include <complex>
#include <cstddef>
#include <optional>
#include <random>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

class NoiseSource : public ISource {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    double amplitude_{0.2};
    double sample_rate_{1e6};
    std::optional<double> duration_sec_;
    uint32_t seed_{42};
    std::mt19937 rng_;
    std::normal_distribution<float> dist_;
    size_t samples_generated_{0};
};

} // namespace archerfish::dsp
