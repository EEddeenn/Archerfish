#include "archerfish/dsp/cw_source.hpp"

#include <cmath>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void CwSource::configure(const nlohmann::json& params) {
    configure_common(params);
    if (params.contains("frequency_hz"))
        frequency_hz_ = params["frequency_hz"].get<double>();
}

void CwSource::prepare() {
    reset_common();
    phase_ = 0.0;
}

size_t CwSource::render_block(std::complex<float>* out, size_t max_samples) {
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;

    const double phase_inc = archerfish::constants::kTwoPi * frequency_hz_ / sample_rate_;
    const float amp = static_cast<float>(amplitude_);
    const double two_pi = archerfish::constants::kTwoPi;

    for (size_t i = 0; i < to_generate; ++i) {
        out[i] = amp * std::complex<float>(static_cast<float>(std::cos(phase_)), static_cast<float>(std::sin(phase_)));
        phase_ += phase_inc;
        if (phase_ >= two_pi) phase_ -= two_pi;
    }

    samples_produced_ += to_generate;
    return to_generate;
}

WaveformMetadata CwSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    meta.nominal_bandwidth = 0.0;
    return meta;
}

void CwSource::reset() {
    reset_common();
    phase_ = 0.0;
}

} // namespace archerfish::dsp
