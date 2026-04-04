#pragma once

#include <complex>
#include <cstddef>
#include <optional>

#include <nlohmann/json.hpp>

namespace archerfish::dsp {

struct WaveformMetadata {
    double nominal_bandwidth{0.0};
    double sample_rate{0.0};
    double peak_amplitude{0.0};
    double rms_amplitude{0.0};
    double crest_factor{0.0};
    std::optional<double> duration_sec;
    bool repeats{false};
};

class ISource {
public:
    ISource() = default;
    virtual ~ISource() = default;
    ISource(const ISource&) = delete;
    ISource& operator=(const ISource&) = delete;
    ISource(ISource&&) = default;
    ISource& operator=(ISource&&) = default;

    virtual void configure(const nlohmann::json& params) = 0;
    virtual void prepare() = 0;
    [[nodiscard]] virtual size_t render_block(std::complex<float>* out, size_t max_samples) = 0;
    virtual WaveformMetadata report_metadata() const = 0;
    virtual void reset() = 0;
};

} // namespace archerfish::dsp
