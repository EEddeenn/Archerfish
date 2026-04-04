#pragma once

#include <complex>
#include <cstddef>
#include <random>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source_base.hpp"

namespace archerfish::dsp {

class OfdmSource : public SourceBase {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    size_t fft_size_{64};
    size_t cyclic_prefix_size_{16};
    size_t active_subcarriers_{60};

    std::mt19937 rng_;
    std::vector<std::complex<float>> symbol_buffer_;
    size_t output_offset_{0};

    static void fft_in_place(std::vector<std::complex<float>>& data, bool inverse);
    static void bit_reversal_permute(std::vector<std::complex<float>>& data);
    void generate_ofdm_symbol();
};

} // namespace archerfish::dsp
