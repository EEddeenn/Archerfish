#include "archerfish/dsp/am_source.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "archerfish/common/constants.hpp"

namespace archerfish::dsp {

void AmSource::configure(const nlohmann::json& params) {
    double next_amplitude = amplitude_;
    double next_carrier_freq_hz = carrier_freq_hz_;
    double next_mod_freq_hz = mod_freq_hz_;
    double next_mod_depth = mod_depth_;

    if (params.contains("amplitude")) {
        next_amplitude = number_param(params, "amplitude");
        validate_non_negative(next_amplitude, "amplitude");
        if (next_amplitude > static_cast<double>(std::numeric_limits<float>::max())) {
            throw std::out_of_range("amplitude exceeds float range");
        }
    }
    if (params.contains("carrier_freq_hz")) {
        next_carrier_freq_hz = number_param(params, "carrier_freq_hz");
        if (!std::isfinite(next_carrier_freq_hz)) {
            throw std::invalid_argument("carrier_freq_hz must be finite");
        }
    }
    if (params.contains("mod_freq_hz")) {
        next_mod_freq_hz = number_param(params, "mod_freq_hz");
        validate_non_negative(next_mod_freq_hz, "mod_freq_hz");
    }
    if (params.contains("mod_depth")) {
        next_mod_depth = number_param(params, "mod_depth");
        validate_non_negative(next_mod_depth, "mod_depth");
        if (next_mod_depth > static_cast<double>(std::numeric_limits<float>::max())) {
            throw std::out_of_range("mod_depth exceeds float range");
        }
    }

    const long double peak = static_cast<long double>(next_amplitude) *
                             (1.0L + static_cast<long double>(next_mod_depth));
    if (peak > static_cast<long double>(std::numeric_limits<float>::max())) {
        throw std::out_of_range("AM peak amplitude exceeds float range");
    }

    configure_common(params);
    carrier_freq_hz_ = next_carrier_freq_hz;
    mod_freq_hz_ = next_mod_freq_hz;
    mod_depth_ = next_mod_depth;
    carrier_phase_ = 0.0;
}

void AmSource::prepare() {
    carrier_phase_ = 0.0;
    reset_common();
}

size_t AmSource::render_block(std::complex<float>* out, size_t max_samples) {
    if (max_samples > 0 && out == nullptr) {
        throw std::invalid_argument("AmSource render output buffer must not be null");
    }
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
        carrier_phase_ = std::fmod(carrier_phase_, two_pi);
        if (carrier_phase_ < 0.0) carrier_phase_ += two_pi;

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
