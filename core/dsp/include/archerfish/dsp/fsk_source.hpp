#pragma once

#include <complex>
#include <cstddef>
#include <random>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source_base.hpp"

namespace archerfish::dsp {

class FskSource : public SourceBase {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    double center_frequency_hz_{0.0};
    double symbol_rate_{1e6};
    int modulation_order_{2};
    double deviation_hz_{5000.0};

    size_t samples_per_symbol_{0};

    std::mt19937 rng_;
    double phase_{0.0};
    size_t samples_within_symbol_{0};
    double current_freq_hz_{0.0};

    void generate_next_symbol();
};

} // namespace archerfish::dsp
