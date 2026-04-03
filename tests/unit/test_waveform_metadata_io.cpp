#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <archerfish/dsp/waveform_metadata_io.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace archerfish::dsp;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::ContainsSubstring;

static std::string make_temp_path(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

static void cleanup(const std::string& base) {
    std::filesystem::remove(base);
    std::filesystem::remove(sidecar_path(base));
}

TEST_CASE("sidecar_path appends .meta.json", "[metadata]") {
    REQUIRE(sidecar_path("output.cf32") == "output.cf32.meta.json");
    REQUIRE(sidecar_path("/tmp/test.cf32") == "/tmp/test.cf32.meta.json");
    REQUIRE(sidecar_path("data.ci16") == "data.ci16.meta.json");
}

TEST_CASE("write_sidecar creates valid JSON file", "[metadata]") {
    auto path = make_temp_path("test_write.cf32");
    cleanup(path);

    WaveformMetadata meta;
    meta.sample_rate = 10e6;
    meta.peak_amplitude = 0.2;
    meta.rms_amplitude = 0.1414;
    meta.crest_factor = 1.414;
    meta.duration_sec = 1.0;
    meta.repeats = false;
    meta.nominal_bandwidth = 0.0;

    bool ok = write_sidecar(path, meta, "cw", 10000000, 80000000);
    REQUIRE(ok);
    REQUIRE(std::filesystem::exists(sidecar_path(path)));

    cleanup(path);
}

TEST_CASE("write then read sidecar preserves all fields", "[metadata]") {
    auto path = make_temp_path("test_roundtrip.cf32");
    cleanup(path);

    WaveformMetadata meta;
    meta.sample_rate = 20e6;
    meta.peak_amplitude = 0.3;
    meta.rms_amplitude = 0.2121;
    meta.crest_factor = 1.4142;
    meta.duration_sec = 0.5;
    meta.repeats = false;
    meta.nominal_bandwidth = 2e6;

    write_sidecar(path, meta, "chirp", 10000000, 80000000);

    auto result = read_sidecar(path);
    REQUIRE(result.has_value());

    auto& data = *result;
    REQUIRE(data.waveform_type == "chirp");
    REQUIRE(data.num_samples == 10000000);
    REQUIRE(data.file_format == "cf32");
    REQUIRE(data.file_size_bytes == 80000000);
    REQUIRE_FALSE(data.created_utc.empty());

    REQUIRE_THAT(data.metadata.sample_rate, WithinAbs(20e6, 1e-6));
    REQUIRE_THAT(data.metadata.peak_amplitude, WithinAbs(0.3, 1e-6));
    REQUIRE_THAT(data.metadata.rms_amplitude, WithinAbs(0.2121, 1e-4));
    REQUIRE_THAT(data.metadata.crest_factor, WithinAbs(1.4142, 1e-4));
    REQUIRE_THAT(data.metadata.nominal_bandwidth, WithinAbs(2e6, 1e-6));
    REQUIRE_FALSE(data.metadata.repeats);
    REQUIRE(data.metadata.duration_sec.has_value());
    REQUIRE_THAT(data.metadata.duration_sec.value(), WithinAbs(0.5, 1e-9));

    cleanup(path);
}

TEST_CASE("read_sidecar returns nullopt for nonexistent file", "[metadata]") {
    auto path = make_temp_path("nonexistent.cf32");
    auto result = read_sidecar(path);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("read_sidecar returns nullopt for invalid JSON", "[metadata]") {
    auto path = make_temp_path("test_invalid.cf32");
    cleanup(path);

    auto meta_path = sidecar_path(path);
    std::ofstream out(meta_path);
    out << "this is not valid json {{{";
    out.close();

    auto result = read_sidecar(path);
    REQUIRE_FALSE(result.has_value());

    cleanup(path);
}

TEST_CASE("read_sidecar returns nullopt for missing waveform key", "[metadata]") {
    auto path = make_temp_path("test_missing_waveform.cf32");
    cleanup(path);

    auto meta_path = sidecar_path(path);
    std::ofstream out(meta_path);
    out << R"({"format_version": 1, "file": {"format": "cf32", "size_bytes": 100}})";
    out.close();

    auto result = read_sidecar(path);
    REQUIRE_FALSE(result.has_value());

    cleanup(path);
}

TEST_CASE("format_sidecar_human contains expected fields", "[metadata]") {
    SidecarData data;
    data.waveform_type = "cw";
    data.num_samples = 10000000;
    data.file_format = "cf32";
    data.file_size_bytes = 80000000;
    data.created_utc = "2026-04-04T12:00:00Z";
    data.metadata.sample_rate = 10e6;
    data.metadata.peak_amplitude = 0.2;
    data.metadata.rms_amplitude = 0.1414;
    data.metadata.crest_factor = 1.414;
    data.metadata.nominal_bandwidth = 0.0;
    data.metadata.duration_sec = 1.0;
    data.metadata.repeats = false;

    auto text = format_sidecar_human(data);

    REQUIRE_THAT(text, ContainsSubstring("cw"));
    REQUIRE_THAT(text, ContainsSubstring("10000000"));
    REQUIRE_THAT(text, ContainsSubstring("cf32"));
    REQUIRE_THAT(text, ContainsSubstring("80000000"));
    REQUIRE_THAT(text, ContainsSubstring("2026-04-04T12:00:00Z"));
}

TEST_CASE("format_sidecar_json produces valid JSON", "[metadata]") {
    SidecarData data;
    data.waveform_type = "qpsk";
    data.num_samples = 4000;
    data.file_format = "cf32";
    data.file_size_bytes = 32000;
    data.created_utc = "2026-04-04T12:00:00Z";
    data.metadata.sample_rate = 8e6;
    data.metadata.peak_amplitude = 0.5;
    data.metadata.rms_amplitude = 0.35;
    data.metadata.crest_factor = 1.43;
    data.metadata.nominal_bandwidth = 1e6;
    data.metadata.duration_sec = 0.0005;
    data.metadata.repeats = false;

    auto json_str = format_sidecar_json(data);
    REQUIRE_THAT(json_str, ContainsSubstring("qpsk"));
    REQUIRE_THAT(json_str, ContainsSubstring("4000"));
    REQUIRE_THAT(json_str, ContainsSubstring("\"format_version\": 1"));
}
