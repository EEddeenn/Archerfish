#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

class SourceBase : public ISource {
public:
    SourceBase() = default;

    void configure_common(const nlohmann::json& params);

    [[nodiscard]] size_t compute_block_size(size_t max_samples) const;

    void fill_common_metadata(WaveformMetadata& meta) const;

    void reset_common();

    double amplitude() const { return amplitude_; }
    double sample_rate() const { return sample_rate_; }
    size_t samples_produced() const { return samples_produced_; }
    uint32_t seed() const { return seed_; }

    static void validate_positive(double value, const char* name);
    static void validate_non_negative(double value, const char* name);

protected:
    double amplitude_{0.2};
    double sample_rate_{1e6};
    std::optional<double> duration_sec_;
    uint32_t seed_{42};
    size_t samples_produced_{0};
};

} // namespace archerfish::dsp
