#include "archerfish/dsp/waveform_metadata_io.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace archerfish::dsp {

namespace {

std::string format_iso8601_utc() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_utc);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string file_extension_to_format(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    if (ext == ".cf32") return "cf32";
    if (ext == ".ci16") return "ci16";
    if (ext == ".ci8") return "ci8";
    return ext.empty() ? "unknown" : ext.substr(1);
}

} // namespace

std::filesystem::path sidecar_path(const std::filesystem::path& waveform_path) {
    return waveform_path.string() + ".meta.json";
}

bool write_sidecar(const std::filesystem::path& waveform_path,
                   const WaveformMetadata& metadata,
                   const std::string& waveform_type,
                   size_t num_samples,
                   size_t file_size_bytes) {
    auto out_path = sidecar_path(waveform_path);

    nlohmann::json wave_obj;
    wave_obj["type"] = waveform_type;
    wave_obj["sample_rate"] = metadata.sample_rate;
    wave_obj["duration_sec"] = metadata.duration_sec.value_or(0.0);
    wave_obj["num_samples"] = num_samples;
    wave_obj["peak_amplitude"] = metadata.peak_amplitude;
    wave_obj["rms_amplitude"] = metadata.rms_amplitude;
    wave_obj["crest_factor"] = metadata.crest_factor;
    wave_obj["nominal_bandwidth"] = metadata.nominal_bandwidth;
    wave_obj["repeats"] = metadata.repeats;

    nlohmann::json file_obj;
    file_obj["format"] = file_extension_to_format(waveform_path);
    file_obj["size_bytes"] = file_size_bytes;

    nlohmann::json root;
    root["format_version"] = 1;
    root["created_utc"] = format_iso8601_utc();
    root["waveform"] = wave_obj;
    root["file"] = file_obj;

    std::ofstream out(out_path);
    if (!out) return false;
    out << root.dump(2) << "\n";
    return static_cast<bool>(out);
}

std::optional<SidecarData> read_sidecar(const std::filesystem::path& waveform_path) {
    auto meta_path = sidecar_path(waveform_path);
    if (!std::filesystem::exists(meta_path)) return std::nullopt;

    std::ifstream in(meta_path);
    if (!in) return std::nullopt;

    nlohmann::json root;
    try {
        in >> root;
    } catch (const nlohmann::json::parse_error&) {
        return std::nullopt;
    }

    if (!root.contains("waveform") || !root["waveform"].is_object()) return std::nullopt;
    if (!root.contains("file") || !root["file"].is_object()) return std::nullopt;

    const auto& wave = root["waveform"];
    const auto& file = root["file"];

    SidecarData data;
    data.waveform_type = wave.value("type", "");
    data.num_samples = wave.value("num_samples", size_t{0});
    data.created_utc = root.value("created_utc", "");
    data.file_format = file.value("format", "");
    data.file_size_bytes = file.value("size_bytes", size_t{0});

    data.metadata.sample_rate = wave.value("sample_rate", 0.0);
    data.metadata.peak_amplitude = wave.value("peak_amplitude", 0.0);
    data.metadata.rms_amplitude = wave.value("rms_amplitude", 0.0);
    data.metadata.crest_factor = wave.value("crest_factor", 0.0);
    data.metadata.nominal_bandwidth = wave.value("nominal_bandwidth", 0.0);
    data.metadata.repeats = wave.value("repeats", false);

    if (wave.contains("duration_sec")) {
        data.metadata.duration_sec = wave["duration_sec"].get<double>();
    }

    return data;
}

std::string format_sidecar_human(const SidecarData& data) {
    double duration = data.metadata.duration_sec.value_or(0.0);
    return fmt::format(
        "Waveform type:    {}\n"
        "Sample rate:      {:.1f} Hz\n"
        "Duration:         {:.6f} s\n"
        "Samples:          {}\n"
        "Peak amplitude:   {:.6f}\n"
        "RMS amplitude:    {:.6f}\n"
        "Crest factor:     {:.6f}\n"
        "Nom. bandwidth:   {:.1f} Hz\n"
        "Repeats:          {}\n"
        "File format:      {}\n"
        "File size:        {} bytes\n"
        "Created (UTC):    {}",
        data.waveform_type,
        data.metadata.sample_rate,
        duration,
        data.num_samples,
        data.metadata.peak_amplitude,
        data.metadata.rms_amplitude,
        data.metadata.crest_factor,
        data.metadata.nominal_bandwidth,
        data.metadata.repeats ? "true" : "false",
        data.file_format,
        data.file_size_bytes,
        data.created_utc);
}

std::string format_sidecar_json(const SidecarData& data) {
    nlohmann::json root;
    root["format_version"] = 1;
    root["created_utc"] = data.created_utc;

    nlohmann::json wave_obj;
    wave_obj["type"] = data.waveform_type;
    wave_obj["sample_rate"] = data.metadata.sample_rate;
    wave_obj["duration_sec"] = data.metadata.duration_sec.value_or(0.0);
    wave_obj["num_samples"] = data.num_samples;
    wave_obj["peak_amplitude"] = data.metadata.peak_amplitude;
    wave_obj["rms_amplitude"] = data.metadata.rms_amplitude;
    wave_obj["crest_factor"] = data.metadata.crest_factor;
    wave_obj["nominal_bandwidth"] = data.metadata.nominal_bandwidth;
    wave_obj["repeats"] = data.metadata.repeats;
    root["waveform"] = wave_obj;

    nlohmann::json file_obj;
    file_obj["format"] = data.file_format;
    file_obj["size_bytes"] = data.file_size_bytes;
    root["file"] = file_obj;

    return root.dump(2);
}

} // namespace archerfish::dsp
