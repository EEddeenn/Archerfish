#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/runtime/render_worker.hpp"

using namespace archerfish::runtime;
using Catch::Matchers::WithinAbs;

TEST_CASE("RenderWorker produces correct blocks for CW", "[runtime][render]") {
    SampleQueue queue(64);
    RenderJob job;
    job.emitter_id = "cw1";
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.01;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    REQUIRE(worker.is_complete());
    size_t expected_samples = static_cast<size_t>(1e6 * 0.01);
    REQUIRE(worker.samples_rendered() == expected_samples);

    size_t total_samples = 0;
    size_t block_count = 0;
    bool first_sob = false;
    bool last_eob = false;

    while (auto block = queue.pop()) {
        block_count++;
        total_samples += block->samples.size();
        if (block_count == 1) first_sob = block->start_of_burst;
        last_eob = block->end_of_burst;
    }

    REQUIRE(block_count > 0);
    REQUIRE(total_samples == expected_samples);
    REQUIRE(first_sob);
    REQUIRE(last_eob);
}

TEST_CASE("RenderWorker samples_rendered matches expected", "[runtime][render]") {
    SampleQueue queue(64);
    RenderJob job;
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.005;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    size_t expected = static_cast<size_t>(1e6 * 0.005);
    REQUIRE(worker.samples_rendered() == expected);
}

TEST_CASE("RenderWorker last block has end_of_burst", "[runtime][render]") {
    SampleQueue queue(64);
    RenderJob job;
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.01;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    bool found_eob = false;
    while (auto block = queue.pop()) {
        if (block->end_of_burst) found_eob = true;
    }
    REQUIRE(found_eob);
}

TEST_CASE("RenderWorker completes after join", "[runtime][render]") {
    SampleQueue queue(64);
    RenderJob job;
    job.waveform_config = nlohmann::json{{"type", "noise"}, {"amplitude", 0.1}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.001;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();
    REQUIRE(worker.is_complete());
}

TEST_CASE("RenderWorker queue receives all blocks in order", "[runtime][render]") {
    SampleQueue queue(128);
    RenderJob job;
    job.waveform_config = nlohmann::json{{"type", "cw"}, {"frequency_hz", 1000.0}, {"amplitude", 0.5}};
    job.sample_rate = 1e6;
    job.duration_sec = 0.01;
    job.block_size = 4096;

    RenderWorker worker(queue, job);
    worker.start();
    worker.join();

    size_t total = 0;
    while (auto block = queue.pop()) {
        total += block->samples.size();
    }

    size_t expected = static_cast<size_t>(1e6 * 0.01);
    REQUIRE(total == expected);
}
