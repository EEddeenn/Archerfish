#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace archerfish::dsp {

enum class WaveformType {
    Unknown,
    CW,
    Chirp,
    Noise,
    BPSK,
    QPSK,
    PSK8,
    QAM16,
    QAM64,
    APSK16,
    APSK32,
    MultiTone,
    File,
    Pulse,
    ASK,
    FSK,
    AM,
    FM,
    PM,
    OFDM
};

/// Convert a WaveformType to its canonical lowercase string name.
[[nodiscard]] std::string to_string(WaveformType t);

/// Parse a string to a WaveformType. Supports canonical names and common aliases
/// (e.g. "16qam" -> QAM16, "64qam" -> QAM64, "8psk" -> PSK8).
/// Returns an error message for unknown strings.
[[nodiscard]] std::expected<WaveformType, std::string> waveform_type_from_string(std::string_view s);

/// Return a CLI-friendly name for help text (uppercase, e.g. "CW", "QPSK", "8PSK").
[[nodiscard]] const char* waveform_type_cli_name(WaveformType t);

} // namespace archerfish::dsp
