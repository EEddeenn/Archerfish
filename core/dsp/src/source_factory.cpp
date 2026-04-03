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
#include "archerfish/dsp/pm_source.hpp"
#include "archerfish/dsp/pulse_source.hpp"

namespace archerfish::dsp {

std::unique_ptr<ISource> create_source(const std::string& type) {
    if (type == "cw") return std::make_unique<CwSource>();
    if (type == "chirp") return std::make_unique<ChirpSource>();
    if (type == "noise") return std::make_unique<NoiseSource>();
    if (type == "multi_tone") return std::make_unique<MultiToneSource>();
    if (type == "file") return std::make_unique<FileSource>();
    if (type == "qpsk" || type == "bpsk" || type == "8psk" || type == "qam16" || type == "qam64") {
        return std::make_unique<ModulatorSource>();
    }
    if (type == "pulse") return std::make_unique<PulseSource>();
    if (type == "ask") return std::make_unique<AskSource>();
    if (type == "fsk") return std::make_unique<FskSource>();
    if (type == "am") return std::make_unique<AmSource>();
    if (type == "fm") return std::make_unique<FmSource>();
    if (type == "pm") return std::make_unique<PmSource>();
    return nullptr;
}

} // namespace archerfish::dsp
