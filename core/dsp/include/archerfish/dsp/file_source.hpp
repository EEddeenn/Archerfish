#pragma once

#include <complex>
#include <cstddef>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source_base.hpp"

namespace archerfish::dsp {

enum class FileFormat {
    CF32,
    CI16
};

[[nodiscard]] FileFormat detect_file_format(const std::string& path);

class FileSource : public SourceBase {
public:
    void configure(const nlohmann::json& params) override;
    void prepare() override;
    size_t render_block(std::complex<float>* out, size_t max_samples) override;
    WaveformMetadata report_metadata() const override;
    void reset() override;

private:
    std::string path_;
    bool loop_{false};

    std::vector<std::complex<float>> data_;
    size_t read_offset_{0};
    bool eof_{false};
};

void write_ci16(const std::string& path, const std::vector<std::complex<float>>& samples);

} // namespace archerfish::dsp
