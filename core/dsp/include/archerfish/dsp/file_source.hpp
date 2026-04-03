#pragma once

#include <complex>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

class FileSource : public ISource {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    std::string path_;
    double sample_rate_{1e6};
    bool loop_{false};
    std::optional<double> duration_sec_;

    std::vector<std::complex<float>> data_;
    size_t read_offset_{0};
    bool eof_{false};
};

} // namespace archerfish::dsp
