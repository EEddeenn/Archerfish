#include "archerfish/dsp/am_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void AmSource::configure(const nlohmann::json& params) {
    configure_common(params);
    if (params.contains("carrier_freq_hz"))
        carrier_freq_hz_ = params["carrier_freq_hz"].get<double>();
    if (params.contains("mod_freq_hz"))
        mod_freq_hz_ = params["mod_freq_hz"].get<double>();
    if (params.contains("mod_depth"))
        mod_depth_ = params["mod_depth"].get<double>();
}

void AmSource::prepare() {
    carrier_phase_ = 0.0;
    reset_common();
}

size_t AmSource::render_block(std::complex<float>* out, size_t max_samples) {
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;

    const double carrier_incr = archerfish::constants::kTwoPi * carrier_freq_hz_ / sample_rate_;
    const double mod_incr = archerfish::constants::kTwoPi * mod_freq_hz_ / sample_rate_;
    const float amp = static_cast<float>(amplitude_);
    const float depth = static_cast<float>(mod_depth_);
    const double two_pi = archerfish::constants::kTwoPi;

    for (size_t i = 0; i < to_generate; ++i) {
        double t_phase = mod_incr * static_cast<double>(samples_produced_);
        float envelope = 1.0f + depth * static_cast<float>(std::cos(t_phase));
        out[i] = amp * envelope * std::complex<float>(static_cast<float>(std::cos(carrier_phase_)), static_cast<float>(std::sin(carrier_phase_)));

        carrier_phase_ += carrier_incr;
        if (carrier_phase_ >= two_pi) carrier_phase_ -= two_pi;

        samples_produced_++;
    }

    return to_generate;
}

WaveformMetadata AmSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_ * (1.0 + mod_depth_);
    meta.rms_amplitude = amplitude_ * std::sqrt(1.0 + mod_depth_ * mod_depth_ / 2.0);
    meta.crest_factor = meta.peak_amplitude / (meta.rms_amplitude > 0.0 ? meta.rms_amplitude : 1.0);
    meta.nominal_bandwidth = 2.0 * mod_freq_hz_;
    return meta;
}

void AmSource::reset() {
    reset_common();
    carrier_phase_ = 0.0;
}

} // namespace archerfish::dsp
