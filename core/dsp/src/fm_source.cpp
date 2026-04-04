#include "archerfish/dsp/fm_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void FmSource::configure(const nlohmann::json& params) {
    configure_common(params);
    if (params.contains("carrier_freq_hz"))
        carrier_freq_hz_ = params["carrier_freq_hz"].get<double>();
    if (params.contains("mod_freq_hz"))
        mod_freq_hz_ = params["mod_freq_hz"].get<double>();
    if (params.contains("deviation_hz"))
        deviation_hz_ = params["deviation_hz"].get<double>();
}

void FmSource::prepare() {
    phase_ = 0.0;
    reset_common();
}

size_t FmSource::render_block(std::complex<float>* out, size_t max_samples) {
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;

    const double carrier_incr = archerfish::constants::kTwoPi * carrier_freq_hz_ / sample_rate_;
    const double mod_incr = archerfish::constants::kTwoPi * mod_freq_hz_ / sample_rate_;
    const double sensitivity = archerfish::constants::kTwoPi * deviation_hz_ / sample_rate_;
    const float amp = static_cast<float>(amplitude_);

    for (size_t i = 0; i < to_generate; ++i) {
        double mod_signal = std::cos(mod_incr * static_cast<double>(samples_produced_));
        phase_ += carrier_incr + sensitivity * mod_signal;

        phase_ = std::fmod(phase_ + archerfish::constants::kPi, archerfish::constants::kTwoPi) - archerfish::constants::kPi;

        out[i] = amp * std::complex<float>(static_cast<float>(std::cos(phase_)),
                                            static_cast<float>(std::sin(phase_)));

        samples_produced_++;
    }

    return to_generate;
}

WaveformMetadata FmSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_ / std::sqrt(2.0);
    meta.crest_factor = std::sqrt(2.0);
    meta.nominal_bandwidth = 2.0 * (deviation_hz_ + mod_freq_hz_);
    return meta;
}

void FmSource::reset() {
    reset_common();
    phase_ = 0.0;
}

} // namespace archerfish::dsp
