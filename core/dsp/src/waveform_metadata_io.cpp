#include "archerfish/dsp/waveform_metadata_io.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>

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

std::optional<size_t> bytes_per_sample_for_format(std::string_view format) {
    if (format == "cf32") return 2 * sizeof(float);
    if (format == "ci16") return 2 * sizeof(int16_t);
    if (format == "ci8") return 2 * sizeof(int8_t);
    return std::nullopt;
}

bool file_size_matches_sample_count(std::string_view format,
                                    size_t num_samples,
                                    size_t file_size_bytes) {
    auto bytes_per_sample = bytes_per_sample_for_format(format);
    if (!bytes_per_sample.has_value()) {
        return true;
    }
    if (num_samples > std::numeric_limits<size_t>::max() / *bytes_per_sample) {
        return false;
    }
    return num_samples * *bytes_per_sample == file_size_bytes;
}

bool is_finite_nonnegative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

bool metadata_is_valid(const WaveformMetadata& metadata) {
    if (!std::isfinite(metadata.sample_rate) || metadata.sample_rate <= 0.0) return false;
    if (!is_finite_nonnegative(metadata.peak_amplitude)) return false;
    if (!is_finite_nonnegative(metadata.rms_amplitude)) return false;
    if (!is_finite_nonnegative(metadata.crest_factor)) return false;
    if (!is_finite_nonnegative(metadata.nominal_bandwidth)) return false;
    if (metadata.duration_sec.has_value() && !is_finite_nonnegative(*metadata.duration_sec)) return false;
    return true;
}

bool read_finite_nonnegative(const nlohmann::json& obj, std::string_view key, double& out) {
    auto it = obj.find(std::string(key));
    if (it == obj.end() || !it->is_number()) return false;
    double value = it->get<double>();
    if (!is_finite_nonnegative(value)) return false;
    out = value;
    return true;
}

bool read_size(const nlohmann::json& obj, std::string_view key, size_t& out) {
    auto it = obj.find(std::string(key));
    if (it == obj.end() || !it->is_number_unsigned()) return false;
    auto value = it->get<uint64_t>();
    if (value > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) return false;
    out = static_cast<size_t>(value);
    return true;
}

bool read_string(const nlohmann::json& obj, std::string_view key, std::string& out) {
    auto it = obj.find(std::string(key));
    if (it == obj.end() || !it->is_string()) return false;
    out = it->get<std::string>();
    return !out.empty();
}

bool read_format_version(const nlohmann::json& root) {
    auto it = root.find("format_version");
    if (it == root.end() || !it->is_number_unsigned()) return false;
    return it->get<uint64_t>() == 1;
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
    if (waveform_path.empty() || waveform_type.empty() || !metadata_is_valid(metadata)) {
        return false;
    }
    auto out_path = sidecar_path(waveform_path);
    const auto file_format = file_extension_to_format(waveform_path);
    if (!file_size_matches_sample_count(file_format, num_samples, file_size_bytes)) {
        return false;
    }

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
    file_obj["format"] = file_format;
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
    std::error_code fs_error;
    if (!std::filesystem::exists(meta_path, fs_error) || fs_error) return std::nullopt;

    std::ifstream in(meta_path);
    if (!in) return std::nullopt;

    nlohmann::json root;
    try {
        in >> root;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }

    if (!read_format_version(root)) return std::nullopt;
    if (!root.contains("waveform") || !root["waveform"].is_object()) return std::nullopt;
    if (!root.contains("file") || !root["file"].is_object()) return std::nullopt;

    const auto& wave = root["waveform"];
    const auto& file = root["file"];

    SidecarData data;
    if (!read_string(wave, "type", data.waveform_type)) return std::nullopt;
    if (!read_size(wave, "num_samples", data.num_samples)) return std::nullopt;
    if (!read_string(root, "created_utc", data.created_utc)) return std::nullopt;
    if (!read_string(file, "format", data.file_format)) return std::nullopt;
    if (!read_size(file, "size_bytes", data.file_size_bytes)) return std::nullopt;
    if (!file_size_matches_sample_count(data.file_format, data.num_samples, data.file_size_bytes)) {
        return std::nullopt;
    }

    if (!read_finite_nonnegative(wave, "sample_rate", data.metadata.sample_rate) ||
        data.metadata.sample_rate <= 0.0) {
        return std::nullopt;
    }
    if (!read_finite_nonnegative(wave, "peak_amplitude", data.metadata.peak_amplitude)) return std::nullopt;
    if (!read_finite_nonnegative(wave, "rms_amplitude", data.metadata.rms_amplitude)) return std::nullopt;
    if (!read_finite_nonnegative(wave, "crest_factor", data.metadata.crest_factor)) return std::nullopt;
    if (!read_finite_nonnegative(wave, "nominal_bandwidth", data.metadata.nominal_bandwidth)) return std::nullopt;
    auto repeats_it = wave.find("repeats");
    if (repeats_it == wave.end() || !repeats_it->is_boolean()) return std::nullopt;
    data.metadata.repeats = repeats_it->get<bool>();

    if (wave.contains("duration_sec")) {
        double duration = 0.0;
        if (!read_finite_nonnegative(wave, "duration_sec", duration)) return std::nullopt;
        data.metadata.duration_sec = duration;
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
