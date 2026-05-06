#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <archerfish/cli/cmd_wave.hpp>

#include <complex>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <string>
#include <unistd.h>

#include <nlohmann/json.hpp>

using namespace archerfish::cli;
using Catch::Matchers::WithinAbs;

static std::string make_temp_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

static void remove_wave_outputs(const std::string& path) {
    std::filesystem::remove(path);
    std::filesystem::remove(path + ".meta.json");
}

static std::string capture_stdout(const std::function<int()>& fn, int& rc) {
    std::fflush(nullptr);

    int pipefd[2];
    REQUIRE(::pipe(pipefd) == 0);

    int saved_stdout = ::dup(STDOUT_FILENO);
    REQUIRE(saved_stdout != -1);
    REQUIRE(::dup2(pipefd[1], STDOUT_FILENO) != -1);
    ::close(pipefd[1]);

    rc = fn();
    std::fflush(nullptr);

    REQUIRE(::dup2(saved_stdout, STDOUT_FILENO) != -1);
    ::close(saved_stdout);

    std::string output;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = ::read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, static_cast<size_t>(bytes_read));
    }
    ::close(pipefd[0]);

    return output;
}

TEST_CASE("wave gen cw produces file with correct sample count", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_cw.cf32");
    double rate = 1e6;
    double duration = 0.01;

    int rc = cmd_wave_gen_cw(opts, rate, duration, 0.5, out);
    REQUIRE(rc == 0);

    size_t expected_samples = static_cast<size_t>(rate * duration);
    size_t file_size = std::filesystem::file_size(out);
    size_t actual_samples = file_size / sizeof(std::complex<float>);
    REQUIRE(actual_samples == expected_samples);

    remove_wave_outputs(out);
}

TEST_CASE("wave gen chirp produces file", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_chirp.cf32");

    int rc = cmd_wave_gen_chirp(opts, 1e6, 0.005, 100e3, 500e3, 0.3, out);
    REQUIRE(rc == 0);
    REQUIRE(std::filesystem::exists(out));
    REQUIRE(std::filesystem::file_size(out) > 0);

    remove_wave_outputs(out);
}

TEST_CASE("wave gen qpsk produces file", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_qpsk.cf32");

    int rc = cmd_wave_gen_qpsk(opts, 1e6, 4, 0.35, 0.001, 0.5, out);
    REQUIRE(rc == 0);
    REQUIRE(std::filesystem::exists(out));
    REQUIRE(std::filesystem::file_size(out) > 0);

    remove_wave_outputs(out);
}

TEST_CASE("wave generation rejects invalid numeric arguments", "[cli][wave]") {
    CliOptions opts;

    auto bad_duration = make_temp_path("test_bad_duration.cf32");
    int rc = cmd_wave_gen_cw(opts, 1e6, -1.0, 0.5, bad_duration);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK_FALSE(std::filesystem::exists(bad_duration));

    auto bad_alpha = make_temp_path("test_bad_alpha.cf32");
    rc = cmd_wave_gen_qpsk(opts, 1e6, 4, 0.0, 0.001, 0.5, bad_alpha);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK_FALSE(std::filesystem::exists(bad_alpha));

    auto bad_ofdm = make_temp_path("test_bad_ofdm.cf32");
    rc = cmd_wave_gen_ofdm(opts, 1e6, 0.001, 0.5, 63, 16, 60, bad_ofdm);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK_FALSE(std::filesystem::exists(bad_ofdm));

    auto bad_nan = make_temp_path("test_bad_nan.cf32");
    rc = cmd_wave_gen_chirp(opts, 1e6, 0.001, std::numeric_limits<double>::quiet_NaN(), 1e3, 0.5, bad_nan);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK_FALSE(std::filesystem::exists(bad_nan));
}

TEST_CASE("wave generation rejects empty output paths", "[cli][wave]") {
    CliOptions opts;

    CHECK(cmd_wave_gen_cw(opts, 1e6, 0.001, 0.5, "") ==
          static_cast<int>(ExitCode::InputValidationFailure));
}

TEST_CASE("wave generation sidecar records streamed sample count", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_streamed_sidecar.cf32");

    int rc = cmd_wave_gen_cw(opts, 1e6, 0.01, 0.5, out);
    REQUIRE(rc == 0);

    std::ifstream f(out + ".meta.json");
    REQUIRE(f.good());
    auto sidecar = nlohmann::json::parse(f);
    CHECK(sidecar["waveform"]["num_samples"] == 10000);
    CHECK(sidecar["file"]["size_bytes"] == 10000 * static_cast<int>(sizeof(std::complex<float>)));

    remove_wave_outputs(out);
}

TEST_CASE("wave generation reports sidecar write failures", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_sidecar_failure.cf32");
    remove_wave_outputs(out);
    std::filesystem::create_directory(out + ".meta.json");

    int rc = cmd_wave_gen_cw(opts, 1e6, 0.001, 0.5, out);
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));

    std::filesystem::remove(out);
    std::filesystem::remove_all(out + ".meta.json");
}

TEST_CASE("wave inspect reports correct metadata for generated file", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_inspect.cf32");
    double rate = 1e6;
    double duration = 0.01;
    double amplitude = 0.5;

    cmd_wave_gen_cw(opts, rate, duration, amplitude, out);

    opts.json_output = true;
    int rc = cmd_wave_inspect(opts, out);
    REQUIRE(rc == 0);

    size_t file_size = std::filesystem::file_size(out);
    size_t actual_samples = file_size / sizeof(std::complex<float>);
    REQUIRE(actual_samples == static_cast<size_t>(rate * duration));

    remove_wave_outputs(out);
}

TEST_CASE("wave inspect ignores stale sidecar metadata", "[cli][wave]") {
    CliOptions opts;
    opts.json_output = true;
    auto out = make_temp_path("test_stale_sidecar.cf32");
    remove_wave_outputs(out);

    {
        std::ofstream f(out, std::ios::binary);
        const std::complex<float> samples[] = {{1.0f, 0.0f}, {0.5f, 0.0f}};
        f.write(reinterpret_cast<const char*>(samples), sizeof(samples));
    }

    {
        std::ofstream f(out + ".meta.json");
        f << R"({
            "format_version": 1,
            "created_utc": "2026-04-04T12:00:00Z",
            "waveform": {
                "type": "cw",
                "sample_rate": 1000000.0,
                "duration_sec": 0.000001,
                "num_samples": 1,
                "peak_amplitude": 1.0,
                "rms_amplitude": 1.0,
                "crest_factor": 1.0,
                "nominal_bandwidth": 0.0,
                "repeats": false
            },
            "file": {"format": "cf32", "size_bytes": 8}
        })";
    }

    int rc = -1;
    auto output = capture_stdout([&]() {
        return cmd_wave_inspect(opts, out);
    }, rc);

    REQUIRE(rc == 0);
    auto parsed = nlohmann::json::parse(output);
    REQUIRE(parsed["samples"] == 2);
    REQUIRE_FALSE(parsed.contains("sidecar"));

    remove_wave_outputs(out);
}

TEST_CASE("wave inspect rejects partial CF32 sample files", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_partial_sample.cf32");
    remove_wave_outputs(out);

    {
        std::ofstream f(out, std::ios::binary);
        const char bytes[] = {1, 2, 3};
        f.write(bytes, sizeof(bytes));
    }

    int rc = cmd_wave_inspect(opts, out);
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));

    remove_wave_outputs(out);
}

TEST_CASE("wave inspect rejects oversized CF32 files before allocation", "[cli][wave]") {
    CliOptions opts;
    auto out = make_temp_path("test_oversized_inspect.cf32");
    remove_wave_outputs(out);

    std::ofstream(out, std::ios::binary).close();
    std::filesystem::resize_file(out,
                                 16'000'001ULL * sizeof(std::complex<float>));

    int rc = cmd_wave_inspect(opts, out);
    REQUIRE(rc == static_cast<int>(ExitCode::GenericFailure));

    remove_wave_outputs(out);
}
