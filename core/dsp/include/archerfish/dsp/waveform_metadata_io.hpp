#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

#include "archerfish/dsp/source.hpp"

namespace archerfish::dsp {

/// Get the sidecar metadata file path for a given waveform file.
/// E.g., "output.cf32" → "output.cf32.meta.json"
std::filesystem::path sidecar_path(const std::filesystem::path& waveform_path);

/// Write sidecar metadata file alongside the waveform file.
/// Returns true on success.
bool write_sidecar(const std::filesystem::path& waveform_path,
                   const WaveformMetadata& metadata,
                   const std::string& waveform_type,
                   size_t num_samples,
                   size_t file_size_bytes);

/// Parsed sidecar metadata.
struct SidecarData {
    WaveformMetadata metadata;
    std::string waveform_type;
    size_t num_samples{0};
    std::string created_utc;
    std::string file_format;
    size_t file_size_bytes{0};
};

/// Read sidecar metadata file. Returns nullopt if not found or invalid.
std::optional<SidecarData> read_sidecar(const std::filesystem::path& waveform_path);

/// Format sidecar data as human-readable string for CLI output.
std::string format_sidecar_human(const SidecarData& data);

/// Format sidecar data as JSON string for CLI --json output.
std::string format_sidecar_json(const SidecarData& data);

} // namespace archerfish::dsp
