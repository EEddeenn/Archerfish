#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/runtime/render_worker.hpp"
#include "archerfish/hal/stub_device.hpp"

using namespace archerfish::runtime;
using namespace archerfish::hal;
using Catch::Matchers::WithinAbs;

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
