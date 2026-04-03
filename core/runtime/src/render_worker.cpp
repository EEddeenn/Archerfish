#include "archerfish/runtime/render_worker.hpp"

#include <algorithm>
#include <thread>

#include "archerfish/dsp/chirp_source.hpp"
#include "archerfish/dsp/cw_source.hpp"
#include "archerfish/dsp/file_source.hpp"
#include "archerfish/dsp/modulator.hpp"
#include "archerfish/dsp/multi_tone_source.hpp"
#include "archerfish/dsp/noise_source.hpp"

namespace archerfish::runtime {

RenderWorker::RenderWorker(SampleQueue& output_queue, const RenderJob& job)
    : queue_(output_queue), job_(job) {}

void RenderWorker::start() {
    thread_ = std::thread(&RenderWorker::run, this);
}

void RenderWorker::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool RenderWorker::is_complete() const {
    return complete_.load(std::memory_order_acquire);
}

size_t RenderWorker::samples_rendered() const {
    return samples_rendered_.load(std::memory_order_acquire);
}

void RenderWorker::run() {
    std::string type = job_.waveform_config.value("type", "");
    auto source = create_source(type);
    if (!source) {
        complete_.store(true, std::memory_order_release);
        return;
    }

    auto config = job_.waveform_config;
    config["sample_rate"] = job_.sample_rate;
    if (job_.duration_sec > 0.0) {
        config["duration_sec"] = job_.duration_sec;
    }

    source->configure(config);
    source->prepare();

    size_t total_target = static_cast<size_t>(job_.sample_rate * job_.duration_sec);
    bool first_block = true;
    std::vector<std::complex<float>> render_buf(job_.block_size);
    SampleBlock block;
    block.samples.reserve(job_.block_size);

    while (samples_rendered_.load(std::memory_order_relaxed) < total_target) {
        size_t remaining = total_target - samples_rendered_.load(std::memory_order_relaxed);
        size_t to_render = std::min(remaining, job_.block_size);

        size_t produced = source->render_block(render_buf.data(), to_render);
        if (produced == 0) break;

        block.samples.assign(render_buf.data(), render_buf.data() + produced);
        block.start_of_burst = first_block;
        first_block = false;

        size_t new_total = samples_rendered_.load(std::memory_order_relaxed) + produced;
        if (new_total >= total_target) {
            block.end_of_burst = true;
        }

        while (!queue_.push(std::move(block))) {
            std::this_thread::yield();
        }
        block.samples.reserve(job_.block_size);

        samples_rendered_.store(new_total, std::memory_order_release);
    }

    if (samples_rendered_.load(std::memory_order_relaxed) < total_target && !first_block) {
        SampleBlock sentinel;
        sentinel.end_of_burst = true;
        sentinel.start_of_burst = false;
        while (!queue_.push(std::move(sentinel))) {
            std::this_thread::yield();
        }
    }

    complete_.store(true, std::memory_order_release);
}

std::unique_ptr<dsp::ISource> RenderWorker::create_source(const std::string& type) {
    if (type == "cw") return std::make_unique<dsp::CwSource>();
    if (type == "chirp") return std::make_unique<dsp::ChirpSource>();
    if (type == "noise") return std::make_unique<dsp::NoiseSource>();
    if (type == "multi_tone") return std::make_unique<dsp::MultiToneSource>();
    if (type == "file") return std::make_unique<dsp::FileSource>();
    if (type == "qpsk" || type == "bpsk" || type == "8psk" || type == "qam16" || type == "qam64") {
        return std::make_unique<dsp::ModulatorSource>();
    }
    return nullptr;
}

} // namespace archerfish::runtime
