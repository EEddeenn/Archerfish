#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <archerfish/dsp/waveform_metadata_io.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
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

TEST_CASE("write_sidecar rejects invalid metadata", "[metadata]") {
    auto path = make_temp_path("test_invalid_write.cf32");
    cleanup(path);

    WaveformMetadata meta;
    meta.sample_rate = 0.0;
    meta.peak_amplitude = 0.2;
    meta.rms_amplitude = 0.1;
    meta.crest_factor = 2.0;
    meta.nominal_bandwidth = 0.0;
    meta.duration_sec = 1.0;

    REQUIRE_FALSE(write_sidecar(path, meta, "cw", 100, 800));
    REQUIRE_FALSE(std::filesystem::exists(sidecar_path(path)));

    meta.sample_rate = 1e6;
    meta.peak_amplitude = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_FALSE(write_sidecar(path, meta, "cw", 100, 800));

    meta.peak_amplitude = 0.2;
    REQUIRE_FALSE(write_sidecar(path, meta, "", 100, 800));

    cleanup(path);
}

TEST_CASE("write_sidecar rejects inconsistent raw file size", "[metadata]") {
    auto path = make_temp_path("test_inconsistent_write.cf32");
    cleanup(path);

    WaveformMetadata meta;
    meta.sample_rate = 1e6;
    meta.peak_amplitude = 0.2;
    meta.rms_amplitude = 0.1;
    meta.crest_factor = 2.0;
    meta.nominal_bandwidth = 0.0;
    meta.duration_sec = 0.001;

    REQUIRE_FALSE(write_sidecar(path, meta, "cw", 100, 799));
    REQUIRE_FALSE(std::filesystem::exists(sidecar_path(path)));

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

TEST_CASE("read_sidecar rejects inconsistent raw file size", "[metadata]") {
    auto path = make_temp_path("test_inconsistent_read.cf32");
    cleanup(path);

    auto meta_path = sidecar_path(path);
    std::ofstream out(meta_path);
    out << R"({
        "format_version": 1,
        "created_utc": "2026-04-04T12:00:00Z",
        "waveform": {
            "type": "cw",
            "sample_rate": 1000000.0,
            "duration_sec": 1.0,
            "num_samples": 100,
            "peak_amplitude": 0.2,
            "rms_amplitude": 0.1,
            "crest_factor": 2.0,
            "nominal_bandwidth": 0.0,
            "repeats": false
        },
        "file": {"format": "cf32", "size_bytes": 799}
    })";
    out.close();

    auto result = read_sidecar(path);
    REQUIRE_FALSE(result.has_value());

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

TEST_CASE("read_sidecar rejects missing or unsupported format versions", "[metadata]") {
    auto path = make_temp_path("test_bad_format_version.cf32");
    cleanup(path);

    auto meta_path = sidecar_path(path);
    auto write_payload = [&](const std::string& format_version_field) {
        std::ofstream out(meta_path);
        out << R"({)"
            << format_version_field
            << R"(
        "created_utc": "2026-04-04T12:00:00Z",
        "waveform": {
            "type": "cw",
            "sample_rate": 1000000.0,
            "duration_sec": 1.0,
            "num_samples": 100,
            "peak_amplitude": 0.2,
            "rms_amplitude": 0.1,
            "crest_factor": 2.0,
            "nominal_bandwidth": 0.0,
            "repeats": false
        },
        "file": {"format": "cf32", "size_bytes": 800}
    })";
    };

    write_payload("");
    CHECK_FALSE(read_sidecar(path).has_value());

    write_payload(R"("format_version": 2,)");
    CHECK_FALSE(read_sidecar(path).has_value());

    write_payload(R"("format_version": "1",)");
    CHECK_FALSE(read_sidecar(path).has_value());

    cleanup(path);
}

TEST_CASE("read_sidecar returns nullopt for invalid field types", "[metadata]") {
    auto path = make_temp_path("test_bad_field_types.cf32");
    cleanup(path);

    auto meta_path = sidecar_path(path);
    std::ofstream out(meta_path);
    out << R"({
        "format_version": 1,
        "created_utc": "2026-04-04T12:00:00Z",
        "waveform": {
            "type": "cw",
            "sample_rate": "1000000",
            "duration_sec": 1.0,
            "num_samples": 100,
            "peak_amplitude": 0.2,
            "rms_amplitude": 0.1,
            "crest_factor": 2.0,
            "nominal_bandwidth": 0.0,
            "repeats": false
        },
        "file": {"format": "cf32", "size_bytes": 800}
    })";
    out.close();

    auto result = read_sidecar(path);
    REQUIRE_FALSE(result.has_value());

    cleanup(path);
}

TEST_CASE("read_sidecar returns nullopt for negative sizes and non-finite metrics", "[metadata]") {
    auto path = make_temp_path("test_bad_numeric_values.cf32");
    cleanup(path);

    auto meta_path = sidecar_path(path);
    std::ofstream out(meta_path);
    out << R"({
        "format_version": 1,
        "created_utc": "2026-04-04T12:00:00Z",
        "waveform": {
            "type": "cw",
            "sample_rate": 1000000.0,
            "duration_sec": 1.0,
            "num_samples": -1,
            "peak_amplitude": 0.2,
            "rms_amplitude": 0.1,
            "crest_factor": 2.0,
            "nominal_bandwidth": 0.0,
            "repeats": false
        },
        "file": {"format": "cf32", "size_bytes": 800}
    })";
    out.close();

    auto negative_result = read_sidecar(path);
    REQUIRE_FALSE(negative_result.has_value());

    std::ofstream out2(meta_path);
    out2 << R"({
        "format_version": 1,
        "created_utc": "2026-04-04T12:00:00Z",
        "waveform": {
            "type": "cw",
            "sample_rate": 1000000.0,
            "duration_sec": -1.0,
            "num_samples": 100,
            "peak_amplitude": 0.2,
            "rms_amplitude": 0.1,
            "crest_factor": 2.0,
            "nominal_bandwidth": 0.0,
            "repeats": false
        },
        "file": {"format": "cf32", "size_bytes": 800}
    })";
    out2.close();

    auto duration_result = read_sidecar(path);
    REQUIRE_FALSE(duration_result.has_value());

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
