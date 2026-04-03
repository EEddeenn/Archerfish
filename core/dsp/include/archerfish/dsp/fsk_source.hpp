#pragma once

#include <complex>
#include <cstddef>
#include <optional>
#include <random>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

class FskSource : public ISource {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    double amplitude_{0.2};
    double center_frequency_hz_{0.0};
    double sample_rate_{1e6};
    double symbol_rate_{1e6};
    int modulation_order_{2};
    double deviation_hz_{5000.0};
    std::optional<double> duration_sec_;
    uint32_t seed_{42};

    size_t samples_per_symbol_{0};

    std::mt19937 rng_;
    double phase_{0.0};
    size_t samples_generated_{0};
    size_t samples_within_symbol_{0};
    double current_freq_hz_{0.0};

    void generate_next_symbol();
};

} // namespace archerfish::dsp
