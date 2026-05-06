#include "archerfish/dsp/pm_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void PmSource::configure(const nlohmann::json& params) {
    double next_carrier_freq_hz = carrier_freq_hz_;
    double next_mod_freq_hz = mod_freq_hz_;
    double next_mod_index = mod_index_;

    if (params.contains("carrier_freq_hz")) {
        next_carrier_freq_hz = number_param(params, "carrier_freq_hz");
        if (!std::isfinite(next_carrier_freq_hz)) {
            throw std::invalid_argument("carrier_freq_hz must be finite");
        }
    }
    if (params.contains("mod_freq_hz")) {
        next_mod_freq_hz = number_param(params, "mod_freq_hz");
        validate_positive(next_mod_freq_hz, "mod_freq_hz");
    }
    if (params.contains("mod_index")) {
        next_mod_index = number_param(params, "mod_index");
        validate_non_negative(next_mod_index, "mod_index");
    }
    configure_common(params);
    carrier_freq_hz_ = next_carrier_freq_hz;
    mod_freq_hz_ = next_mod_freq_hz;
    mod_index_ = next_mod_index;
    carrier_phase_ = 0.0;
}

void PmSource::prepare() {
    carrier_phase_ = 0.0;
    reset_common();
}

size_t PmSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("PmSource render output buffer must not be null");
    }
    size_t to_generate = compute_block_size(max_samples);
    if (to_generate == 0)
        return 0;

    const double carrier_incr = archerfish::constants::kTwoPi * carrier_freq_hz_ / sample_rate_;
    const double mod_incr = archerfish::constants::kTwoPi * mod_freq_hz_ / sample_rate_;
    const float amp = static_cast<float>(amplitude_);
    const double idx = mod_index_;
    const double two_pi = archerfish::constants::kTwoPi;

    for (size_t i = 0; i < to_generate; ++i) {
        double phase_modulation = idx * std::cos(mod_incr * static_cast<double>(samples_produced_));
        double total_phase = carrier_phase_ + phase_modulation;

        out[i] = amp * std::complex<float>(static_cast<float>(std::cos(total_phase)), static_cast<float>(std::sin(total_phase)));

        carrier_phase_ += carrier_incr;
        carrier_phase_ = std::fmod(carrier_phase_, two_pi);
        if (carrier_phase_ < 0.0) carrier_phase_ += two_pi;

        samples_produced_++;
    }

    return to_generate;
}

WaveformMetadata PmSource::report_metadata() const {
    WaveformMetadata meta;
    fill_common_metadata(meta);
    meta.peak_amplitude = amplitude_;
    meta.rms_amplitude = amplitude_;
    meta.crest_factor = amplitude_ > 0.0 ? 1.0 : 0.0;
    meta.nominal_bandwidth = 2.0 * (mod_index_ + 1.0) * mod_freq_hz_;
    return meta;
}

void PmSource::reset() {
    reset_common();
    carrier_phase_ = 0.0;
}

} // namespace archerfish::dsp
