#include "archerfish/dsp/waveform_type.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace archerfish::dsp {

namespace {

struct Mapping {
    WaveformType type;
    const char* canonical;
    const char* cli_name;
};

constexpr Mapping kMappings[] = {
    {WaveformType::CW,         "cw",         "CW"},
    {WaveformType::Chirp,      "chirp",      "Chirp"},
    {WaveformType::Noise,      "noise",      "Noise"},
    {WaveformType::BPSK,       "bpsk",       "BPSK"},
    {WaveformType::QPSK,       "qpsk",       "QPSK"},
    {WaveformType::PSK8,       "8psk",       "8PSK"},
    {WaveformType::QAM16,      "qam16",      "QAM16"},
    {WaveformType::QAM64,      "qam64",      "QAM64"},
    {WaveformType::APSK16,     "apsk16",     "16-APSK"},
    {WaveformType::APSK32,     "apsk32",     "32-APSK"},
    {WaveformType::MultiTone,  "multi_tone", "MultiTone"},
    {WaveformType::File,       "file",       "File"},
    {WaveformType::Pulse,      "pulse",      "Pulse"},
    {WaveformType::ASK,        "ask",        "ASK"},
    {WaveformType::FSK,        "fsk",        "FSK"},
    {WaveformType::AM,         "am",         "AM"},
    {WaveformType::FM,         "fm",         "FM"},
    {WaveformType::PM,         "pm",         "PM"},
    {WaveformType::OFDM,       "ofdm",       "OFDM"},
};

} // namespace

std::string to_string(WaveformType t) {
    for (const auto& m : kMappings) {
        if (m.type == t) return m.canonical;
    }
    return "unknown";
}

std::expected<WaveformType, std::string> waveform_type_from_string(std::string_view s) {
    std::string lower;
    lower.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isspace(c) || c == '-' || c == '_') continue;
        lower.push_back(static_cast<char>(std::tolower(c)));
    }

    for (const auto& m : kMappings) {
        std::string canonical;
        canonical.reserve(std::char_traits<char>::length(m.canonical));
        for (unsigned char c : std::string_view(m.canonical)) {
            if (c == '_') continue;
            canonical.push_back(static_cast<char>(std::tolower(c)));
        }
        if (lower == canonical) return m.type;
    }

    if (lower == "psk8") return WaveformType::PSK8;
    if (lower == "16qam") return WaveformType::QAM16;
    if (lower == "64qam") return WaveformType::QAM64;
    if (lower == "16apsk") return WaveformType::APSK16;
    if (lower == "32apsk") return WaveformType::APSK32;

    return std::unexpected(std::string("Unknown waveform type: '") + std::string(s) + "'");
}

const char* waveform_type_cli_name(WaveformType t) {
    for (const auto& m : kMappings) {
        if (m.type == t) return m.cli_name;
    }
    return "Unknown";
}

} // namespace archerfish::dsp
