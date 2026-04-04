#pragma once

#include <complex>
#include <cstddef>
#include <random>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source_base.hpp"

namespace archerfish::dsp {

class AskSource : public SourceBase {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    double frequency_hz_{0.0};
    double symbol_rate_{1e6};
    int num_levels_{2};

    size_t samples_per_symbol_{0};

    std::mt19937 rng_;
    double phase_{0.0};
    size_t samples_within_symbol_{0};
    float current_symbol_value_{0.0f};

    void generate_next_symbol();
};

} // namespace archerfish::dsp
