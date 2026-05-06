#include "archerfish/dsp/cw_source.hpp"

#include <cmath>
#include <stdexcept>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void CwSource::configure(const nlohmann::json& params) {
    double next_frequency_hz = frequency_hz_;
    if (params.contains("frequency_hz")) {
        next_frequency_hz = number_param(params, "frequency_hz");
        if (!std::isfinite(next_frequency_hz)) {
            throw std::invalid_argument("frequency_hz must be finite");
        }
    }
    configure_common(params);
    frequency_hz_ = next_frequency_hz;
    phase_ = 0.0;
}

void CwSource::prepare() {
    reset_common();
    phase_ = 0.0;
}

size_t CwSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("CwSource render output buffer must not be null");
    }
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;

    const double phase_inc = archerfish::constants::kTwoPi * frequency_hz_ / sample_rate_;
    const float amp = static_cast<float>(amplitude_);
    const double two_pi = archerfish::constants::kTwoPi;

    for (size_t i = 0; i < to_generate; ++i) {
        out[i] = amp * std::complex<float>(static_cast<float>(std::cos(phase_)), static_cast<float>(std::sin(phase_)));
        phase_ += phase_inc;
        phase_ = std::fmod(phase_, two_pi);
        if (phase_ < 0.0) phase_ += two_pi;
    }

    samples_produced_ += to_generate;
    return to_generate;
}

WaveformMetadata CwSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_;
    meta.crest_factor = amplitude_ > 0.0 ? 1.0 : 0.0;
    meta.nominal_bandwidth = 0.0;
    return meta;
}

void CwSource::reset() {
    reset_common();
    phase_ = 0.0;
}

} // namespace archerfish::dsp
