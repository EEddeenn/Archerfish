#include "archerfish/runtime/render_worker.hpp"

#include <algorithm>
#include <thread>

#include <spdlog/spdlog.h>

#include "archerfish/dsp/source_factory.hpp"
#include "archerfish/dsp/waveform_type.hpp"

#include "archerfish/impairments/impairment_chain.hpp"

namespace archerfish::runtime {

RenderWorker::RenderWorker(SampleQueue& output_queue, const RenderJob& job,
                           std::stop_source stop_src)
    : queue_(output_queue), job_(job), stop_source_(std::move(stop_src)) {}

void RenderWorker::start() {
    thread_ = std::thread(&RenderWorker::run, this);
}

void RenderWorker::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void RenderWorker::request_stop() {
    stop_source_.request_stop();
    queue_.notify_all();
}

bool RenderWorker::is_complete() const {
    return complete_.load(std::memory_order_acquire);
}

size_t RenderWorker::samples_rendered() const {
    return samples_rendered_.load(std::memory_order_acquire);
}

void RenderWorker::run() {
    auto stoken = stop_source_.get_token();

    std::string type_str = job_.waveform_config.value("type", "");
    auto type_result = dsp::waveform_type_from_string(type_str);
    if (!type_result.has_value()) {
        complete_.store(true, std::memory_order_release);
        queue_.notify_all();
        return;
    }
    auto source = dsp::create_source(*type_result);
    if (!source) {
        complete_.store(true, std::memory_order_release);
        queue_.notify_all();
        return;
    }

    auto config = job_.waveform_config;
    config["sample_rate"] = job_.sample_rate;
    if (job_.duration_sec > 0.0) {
        config["duration_sec"] = job_.duration_sec;
    }

    source->configure(config);
    source->prepare();

    std::unique_ptr<impairments::ImpairmentChain> chain;
    if (job_.impairments.has_value()) {
        chain = impairments::build_chain(*job_.impairments, job_.sample_rate);
    }

    size_t total_target = static_cast<size_t>(job_.sample_rate * job_.duration_sec);
    bool first_block = true;

    while (samples_rendered_.load(std::memory_order_relaxed) < total_target) {
        if (stoken.stop_requested()) break;

        size_t remaining = total_target - samples_rendered_.load(std::memory_order_relaxed);
        size_t to_render = std::min(remaining, job_.block_size);

        SampleBlock block;
        block.samples.resize(to_render);

        size_t produced = source->render_block(block.samples.data(), to_render);
        if (produced == 0) break;

        if (chain) {
            chain->apply(block.samples.data(), produced);
        }

        block.samples.resize(produced);
        block.start_of_burst = first_block;
        first_block = false;

        size_t new_total = samples_rendered_.load(std::memory_order_relaxed) + produced;
        if (new_total >= total_target) {
            block.end_of_burst = true;
        }

        if (!queue_.push_wait(std::move(block), stoken)) break;

        samples_rendered_.store(new_total, std::memory_order_release);
    }

    if (!stoken.stop_requested() &&
        samples_rendered_.load(std::memory_order_relaxed) < total_target && !first_block) {
        SampleBlock sentinel;
        sentinel.end_of_burst = true;
        sentinel.start_of_burst = false;
        queue_.push_wait(std::move(sentinel), stoken);
    }

    spdlog::info("Render complete: {} samples, target {}", samples_rendered_.load(), total_target);
    complete_.store(true, std::memory_order_release);
    queue_.notify_all();
}

std::unique_ptr<dsp::ISource> RenderWorker::create_source(dsp::WaveformType type) {
    return dsp::create_source(type);
}

std::vector<std::complex<float>> RenderWorker::pre_render(const RenderJob& job) {
    std::string type_str = job.waveform_config.value("type", "");
    auto type_result = dsp::waveform_type_from_string(type_str);
    if (!type_result.has_value()) return {};

    auto source = dsp::create_source(*type_result);
    if (!source) return {};

    auto config = job.waveform_config;
    config["sample_rate"] = job.sample_rate;
    if (job.duration_sec > 0.0) {
        config["duration_sec"] = job.duration_sec;
    }

    source->configure(config);
    source->prepare();

    std::unique_ptr<impairments::ImpairmentChain> chain;
    if (job.impairments.has_value()) {
        chain = impairments::build_chain(*job.impairments, job.sample_rate);
    }

    size_t total_target = static_cast<size_t>(job.sample_rate * job.duration_sec);
    std::vector<std::complex<float>> buffer;
    buffer.reserve(total_target);

    size_t rendered = 0;
    while (rendered < total_target) {
        size_t remaining = total_target - rendered;
        size_t to_render = std::min(remaining, job.block_size);
        std::vector<std::complex<float>> block(to_render);

        size_t produced = source->render_block(block.data(), to_render);
        if (produced == 0) break;

        if (chain) {
            chain->apply(block.data(), produced);
        }

        for (size_t i = 0; i < produced; ++i) {
            buffer.push_back(block[i]);
        }
        rendered += produced;
    }

    return buffer;
}

} // namespace archerfish::runtime
