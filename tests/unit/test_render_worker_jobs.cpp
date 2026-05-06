#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/runtime/render_worker.hpp"
#include "archerfish/hal/stub_device.hpp"

#include <complex>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

using namespace archerfish::runtime;
using namespace archerfish::hal;
using Catch::Matchers::WithinAbs;

namespace {

std::string write_short_cf32_fixture(const char* name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    const std::vector<std::complex<float>> samples{
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {-1.0f, 0.0f},
        {0.0f, -1.0f},
        {0.5f, 0.5f},
        {-0.5f, 0.5f},
        {-0.5f, -0.5f},
        {0.5f, -0.5f},
        {0.25f, 0.0f},
        {0.0f, 0.25f},
    };

    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out.write(reinterpret_cast<const char*>(samples.data()),
              static_cast<std::streamsize>(samples.size() * sizeof(std::complex<float>)));
    REQUIRE(out.good());
    return path.string();
}

} // namespace

TEST_CASE("RenderWorker noise waveform renders all samples", "[runtime][render_jobs]") {
    SampleQueue queue(64);
    RenderJob job;
    job.emitter_id = "noise1";
    job.waveform_config = nlohmann::json{{"type", "noise"}, {"amplitude", 0.1}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.01;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    REQUIRE(worker.is_complete());
    size_t expected = static_cast<size_t>(1e6 * 0.01);
    REQUIRE(worker.samples_rendered() == expected);
}

TEST_CASE("RenderWorker chirp waveform renders all samples", "[runtime][render_jobs]") {
    SampleQueue queue(64);
    RenderJob job;
    job.emitter_id = "chirp1";
    job.waveform_config = nlohmann::json{{"type", "chirp"}, {"f0_hz", -1e6}, {"f1_hz", 1e6}, {"amplitude", 0.3}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.005;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    REQUIRE(worker.is_complete());
    size_t expected = static_cast<size_t>(1e6 * 0.005);
    REQUIRE(worker.samples_rendered() == expected);
}

TEST_CASE("RenderWorker very short duration completes", "[runtime][render_jobs]") {
    SampleQueue queue(64);
    RenderJob job;
    job.emitter_id = "short1";
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.0001;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    REQUIRE(worker.is_complete());
    REQUIRE(worker.samples_rendered() == 100);
}

TEST_CASE("RenderWorker rounds target sample count like DSP sources", "[runtime][render_jobs]") {
    SampleQueue queue(64);
    RenderJob job;
    job.emitter_id = "rounding";
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 0.0}, {"amplitude", 0.5}};
    job.sample_rate = 1000.0;
    job.duration_sec = 0.0015;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    REQUIRE(worker.is_complete());
    REQUIRE(worker.samples_rendered() == 2);

    size_t queued_samples = 0;
    while (auto block = queue.pop()) {
        queued_samples += block->samples.size();
    }
    REQUIRE(queued_samples == 2);
}

TEST_CASE("RenderWorker pre_render produces correct sample count", "[runtime][render_jobs]") {
    RenderJob job;
    job.emitter_id = "pre_render_test";
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.01;
    job.block_size = 4096;

    auto samples = RenderWorker::pre_render(job);
    size_t expected = static_cast<size_t>(1e6 * 0.01);
    REQUIRE(samples.size() == expected);
}

TEST_CASE("RenderWorker pre_render rounds target sample count like DSP sources", "[runtime][render_jobs]") {
    RenderJob job;
    job.emitter_id = "pre_render_rounding";
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 0.0}, {"amplitude", 0.5}};
    job.sample_rate = 1000.0;
    job.duration_sec = 0.0015;
    job.block_size = 4096;

    auto samples = RenderWorker::pre_render(job);
    REQUIRE(samples.size() == 2);
}

TEST_CASE("RenderWorker pre_render noise produces nonzero output", "[runtime][render_jobs]") {
    RenderJob job;
    job.emitter_id = "pre_noise";
    job.waveform_config = nlohmann::json{{"type", "noise"}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.001;
    job.block_size = 4096;

    auto samples = RenderWorker::pre_render(job);
    bool has_nonzero = false;
    for (const auto& s : samples) {
        if (s.real() != 0.0f || s.imag() != 0.0f) {
            has_nonzero = true;
            break;
        }
    }
    REQUIRE(has_nonzero);
}

TEST_CASE("RenderWorker fails when a finite source ends before the target sample count", "[runtime][render_jobs]") {
    SampleQueue queue(64);
    const auto path = write_short_cf32_fixture("archerfish_short_file_render.cf32");
    RenderJob job;
    job.emitter_id = "short_file_render";
    job.waveform_config = nlohmann::json{{"type", "file"},
                                         {"path", path},
                                         {"loop", false}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.000020;
    job.block_size = 8;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    REQUIRE(worker.is_complete());
    REQUIRE(worker.has_failed());
    REQUIRE(worker.samples_rendered() == 10);
}

TEST_CASE("RenderWorker pre_render rejects incomplete finite sources", "[runtime][render_jobs]") {
    const auto path = write_short_cf32_fixture("archerfish_short_file_pre_render.cf32");
    RenderJob job;
    job.emitter_id = "short_file_pre_render";
    job.waveform_config = nlohmann::json{{"type", "file"},
                                         {"path", path},
                                         {"loop", false}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.000020;
    job.block_size = 8;

    auto samples = RenderWorker::pre_render(job);
    REQUIRE(samples.empty());
}

TEST_CASE("RenderWorker completes cleanly for invalid render jobs", "[runtime][render_jobs]") {
    SampleQueue queue(64);
    RenderJob job;
    job.emitter_id = "bad_render";
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = std::numeric_limits<double>::quiet_NaN();
    job.duration_sec = 0.001;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    REQUIRE(worker.is_complete());
    REQUIRE(worker.has_failed());
    REQUIRE(worker.samples_rendered() == 0);
    REQUIRE_FALSE(queue.pop().has_value());
}

TEST_CASE("RenderWorker rejects oversized block size", "[runtime][render_jobs]") {
    SampleQueue queue(64);
    RenderJob job;
    job.emitter_id = "huge_block";
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.001;
    job.block_size = 16'000'001;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    REQUIRE(worker.is_complete());
    REQUIRE(worker.has_failed());
    REQUIRE(worker.samples_rendered() == 0);
    REQUIRE_FALSE(queue.pop().has_value());
}

TEST_CASE("RenderWorker completes cleanly for non-string waveform type", "[runtime][render_jobs]") {
    SampleQueue queue(64);
    RenderJob job;
    job.emitter_id = "bad_type";
    job.waveform_config = nlohmann::json{{"type", 42}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.001;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    REQUIRE(worker.is_complete());
    REQUIRE(worker.has_failed());
    REQUIRE(worker.samples_rendered() == 0);
    REQUIRE_FALSE(queue.pop().has_value());
}

TEST_CASE("RenderWorker pre_render returns empty for invalid source config", "[runtime][render_jobs]") {
    RenderJob job;
    job.emitter_id = "bad_pre_render";
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", -1.0}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.001;
    job.block_size = 4096;

    auto samples = RenderWorker::pre_render(job);
    REQUIRE(samples.empty());
}

TEST_CASE("RenderWorker pre_render returns empty for oversized jobs", "[runtime][render_jobs]") {
    RenderJob job;
    job.emitter_id = "huge_pre_render";
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 20.0;
    job.block_size = 4096;

    auto samples = RenderWorker::pre_render(job);
    REQUIRE(samples.empty());

    job.duration_sec = 0.001;
    job.block_size = 16'000'001;
    samples = RenderWorker::pre_render(job);
    REQUIRE(samples.empty());
}

TEST_CASE("RenderWorker pre_render returns empty for non-string waveform type", "[runtime][render_jobs]") {
    RenderJob job;
    job.emitter_id = "bad_pre_render_type";
    job.waveform_config = nlohmann::json{{"type", false}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.001;
    job.block_size = 4096;

    auto samples = RenderWorker::pre_render(job);
    REQUIRE(samples.empty());
}
