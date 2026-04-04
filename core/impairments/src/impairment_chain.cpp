#include "archerfish/impairments/impairment_chain.hpp"

#include <complex>
#include <cstddef>
#include <stdexcept>

#include "archerfish/impairments/amplitude_ripple.hpp"
#include "archerfish/impairments/awgn.hpp"
#include "archerfish/impairments/burst_dropout.hpp"
#include "archerfish/impairments/cfo.hpp"
#include "archerfish/impairments/dc_offset.hpp"
#include "archerfish/impairments/delay.hpp"
#include "archerfish/impairments/fading.hpp"
#include "archerfish/impairments/iq_imbalance.hpp"
#include "archerfish/impairments/multipath.hpp"
#include "archerfish/impairments/pa_nonlinearity.hpp"
#include "archerfish/impairments/phase_offset.hpp"
#include "archerfish/impairments/phase_noise.hpp"
#include "archerfish/scenario/scenario.hpp"

namespace archerfish::impairments {

void ImpairmentChain::add(std::unique_ptr<IImpairment> impairment) {
    chain_.push_back(std::move(impairment));
}

void ImpairmentChain::apply(std::complex<float>* data, size_t count) {
    for (auto& impairment : chain_) {
        impairment->apply(data, count);
    }
}

size_t ImpairmentChain::size() const {
    return chain_.size();
}

IImpairment& ImpairmentChain::at(size_t index) {
    if (index >= chain_.size()) {
        throw std::out_of_range("ImpairmentChain index out of range");
    }
    return *chain_[index];
}

void ImpairmentChain::set_enabled(size_t index, bool enabled) {
    at(index).set_enabled(enabled);
}

void ImpairmentChain::clear() {
    chain_.clear();
}

std::unique_ptr<ImpairmentChain> build_chain(
    const scenario::ImpairmentSettings& imp, double sample_rate) {
    auto chain = std::make_unique<ImpairmentChain>();
    if (imp.cfo_hz.has_value())
        chain->add(std::make_unique<CfoImpairment>(*imp.cfo_hz, sample_rate));
    if (imp.awgn_power.has_value())
        chain->add(std::make_unique<AwgnImpairment>(*imp.awgn_power));
    if (imp.phase_offset_rad.has_value())
        chain->add(std::make_unique<PhaseOffsetImpairment>(*imp.phase_offset_rad));
    if (imp.dc_offset_i.has_value() || imp.dc_offset_q.has_value())
        chain->add(std::make_unique<DcOffsetImpairment>(
            imp.dc_offset_i.value_or(0.0), imp.dc_offset_q.value_or(0.0)));
    if (imp.iq_gain_imbalance_db.has_value() || imp.iq_phase_imbalance_rad.has_value())
        chain->add(std::make_unique<IqImbalanceImpairment>(
            imp.iq_gain_imbalance_db.value_or(0.0), imp.iq_phase_imbalance_rad.value_or(0.0)));
    if (imp.amplitude_ripple_db.has_value())
        chain->add(std::make_unique<AmplitudeRippleImpairment>(
            *imp.amplitude_ripple_db, imp.amplitude_ripple_freq_hz.value_or(1000.0), sample_rate));
    if (imp.delay_sec.has_value())
        chain->add(std::make_unique<DelayImpairment>(*imp.delay_sec, sample_rate));
    if (imp.burst_dropout_rate.has_value())
        chain->add(std::make_unique<BurstDropoutImpairment>(
            *imp.burst_dropout_rate, imp.burst_dropout_mean_burst_sec.value_or(0.001)));
    if (imp.phase_noise_bandwidth_hz.has_value() && imp.phase_noise_magnitude_rad.has_value())
        chain->add(std::make_unique<PhaseNoiseImpairment>(
            *imp.phase_noise_bandwidth_hz, *imp.phase_noise_magnitude_rad,
            sample_rate, imp.phase_noise_psd_shape.value_or("1f")));
    if (imp.multipath_delay_samples.has_value() && imp.multipath_amplitude.has_value())
        chain->add(std::make_unique<MultipathImpairment>(
            static_cast<size_t>(*imp.multipath_delay_samples),
            static_cast<float>(*imp.multipath_amplitude)));
    if (imp.fading_doppler_hz.has_value())
        chain->add(std::make_unique<FadingImpairment>(
            *imp.fading_doppler_hz, sample_rate,
            imp.fading_type.value_or("rayleigh"),
            imp.fading_k_factor.value_or(0.0)));
    if (imp.pa_model.has_value())
        chain->add(std::make_unique<PaNonlinearityImpairment>(
            *imp.pa_model,
            imp.pa_saturation.value_or(1.0),
            imp.pa_smoothness.value_or(2.0),
            imp.pa_phase_shift.value_or(0.0)));
    if (chain->size() == 0) return nullptr;
    return chain;
}

} // namespace archerfish::impairments
