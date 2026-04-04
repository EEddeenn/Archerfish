#include "archerfish/dsp/source_factory.hpp"

#include "archerfish/dsp/am_source.hpp"
#include "archerfish/dsp/ask_source.hpp"
#include "archerfish/dsp/chirp_source.hpp"
#include "archerfish/dsp/cw_source.hpp"
#include "archerfish/dsp/file_source.hpp"
#include "archerfish/dsp/fm_source.hpp"
#include "archerfish/dsp/fsk_source.hpp"
#include "archerfish/dsp/modulator.hpp"
#include "archerfish/dsp/multi_tone_source.hpp"
#include "archerfish/dsp/noise_source.hpp"
#include "archerfish/dsp/ofdm_source.hpp"
#include "archerfish/dsp/pm_source.hpp"
#include "archerfish/dsp/pulse_source.hpp"

namespace archerfish::dsp {

std::unique_ptr<ISource> create_source(WaveformType type) {
    switch (type) {
        case WaveformType::Unknown:   return nullptr;
        case WaveformType::CW:        return std::make_unique<CwSource>();
        case WaveformType::Chirp:     return std::make_unique<ChirpSource>();
        case WaveformType::Noise:     return std::make_unique<NoiseSource>();
        case WaveformType::MultiTone: return std::make_unique<MultiToneSource>();
        case WaveformType::File:      return std::make_unique<FileSource>();
        case WaveformType::BPSK:
        case WaveformType::QPSK:
        case WaveformType::PSK8:
        case WaveformType::QAM16:
        case WaveformType::QAM64:
        case WaveformType::APSK16:
        case WaveformType::APSK32:
            return std::make_unique<ModulatorSource>();
        case WaveformType::OFDM:
            return std::make_unique<OfdmSource>();
        case WaveformType::Pulse:     return std::make_unique<PulseSource>();
        case WaveformType::ASK:       return std::make_unique<AskSource>();
        case WaveformType::FSK:       return std::make_unique<FskSource>();
        case WaveformType::AM:        return std::make_unique<AmSource>();
        case WaveformType::FM:        return std::make_unique<FmSource>();
        case WaveformType::PM:        return std::make_unique<PmSource>();
    }
    return nullptr;
}

std::unique_ptr<ISource> create_source(const std::string& type) {
    auto result = waveform_type_from_string(type);
    if (!result.has_value()) return nullptr;
    return create_source(*result);
}

} // namespace archerfish::dsp
