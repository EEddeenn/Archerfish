#pragma once

#include <complex>
#include <cstddef>
#include <optional>
#include <random>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

enum class ModulationType { BPSK, QPSK, PSK8, QAM16, QAM64 };

class ModulatorSource : public ISource {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    ModulationType modulation_{ModulationType::BPSK};
    double symbol_rate_{1e6};
    size_t samples_per_symbol_{4};
    double rrc_alpha_{0.35};
    double amplitude_{0.2};
    double sample_rate_{1e6};
    std::optional<double> duration_sec_;
    uint32_t seed_{42};

    std::mt19937 rng_;
    std::vector<std::complex<float>> constellation_;
    std::vector<float> rrc_taps_;
    std::vector<std::complex<float>> symbol_buffer_;
    std::vector<std::complex<float>> shaped_buffer_;
    size_t output_offset_{0};
    size_t samples_produced_{0};

    void build_constellation();
    void build_rrc_taps();
    std::complex<float> map_symbol(uint32_t bits);
    uint32_t random_bits();
};

} // namespace archerfish::dsp
