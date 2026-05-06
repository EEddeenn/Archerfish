#include "archerfish/runtime/render_worker.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <stdexcept>
#include <thread>

#include <spdlog/spdlog.h>

#include "archerfish/dsp/source_factory.hpp"
#include "archerfish/dsp/source_base.hpp"
#include "archerfish/dsp/waveform_type.hpp"

#include "archerfish/impairments/impairment_chain.hpp"

namespace archerfish::runtime {

namespace {

constexpr size_t kMaxRenderBlockSamples = 16'000'000;
constexpr size_t kMaxPreRenderSamples = 16'000'000;

std::optional<size_t> render_target_samples(double sample_rate, double duration_sec) {
    try {
        return dsp::SourceBase::checked_sample_count(sample_rate, duration_sec);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string waveform_type_string(const nlohmann::json& config) {
    if (!config.contains("type") || !config.at("type").is_string()) {
        return {};
    }
    return config.at("type").get<std::string>();
}

} // namespace

RenderWorker::RenderWorker(SampleQueue& output_queue, const RenderJob& job,
                           std::stop_source stop_src)
    : queue_(output_queue), job_(job), stop_source_(std::move(stop_src)) {}

RenderWorker::~RenderWorker() {
    if (thread_.joinable()) {
        request_stop();
    }
    join();
}

void RenderWorker::start() {
    if (thread_.joinable()) {
        throw std::logic_error("RenderWorker is already running");
    }
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

bool RenderWorker::has_failed() const {
    return failed_.load(std::memory_order_acquire);
}

size_t RenderWorker::samples_rendered() const {
    return samples_rendered_.load(std::memory_order_acquire);
}

void RenderWorker::run() {
    auto finish = [this] {
        complete_.store(true, std::memory_order_release);
        queue_.notify_all();
    };

    try {
    auto stoken = stop_source_.get_token();

    if (job_.block_size == 0) {
        spdlog::error("Render job '{}' has zero block size", job_.emitter_id);
        failed_.store(true, std::memory_order_release);
        finish();
        return;
    }
    if (job_.block_size > kMaxRenderBlockSamples) {
        spdlog::error("Render job '{}' block size is too large", job_.emitter_id);
        failed_.store(true, std::memory_order_release);
        finish();
        return;
    }

    auto total_target_opt = render_target_samples(job_.sample_rate, job_.duration_sec);
    if (!total_target_opt.has_value()) {
        spdlog::error("Render job '{}' has invalid rate or duration", job_.emitter_id);
        failed_.store(true, std::memory_order_release);
        finish();
        return;
    }

    std::string type_str = waveform_type_string(job_.waveform_config);
    auto type_result = dsp::waveform_type_from_string(type_str);
    if (!type_result.has_value()) {
        failed_.store(true, std::memory_order_release);
        finish();
        return;
    }
    auto source = dsp::create_source(*type_result);
    if (!source) {
        failed_.store(true, std::memory_order_release);
        finish();
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

    size_t total_target = *total_target_opt;
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

    const size_t rendered = samples_rendered_.load(std::memory_order_relaxed);
    if (!stoken.stop_requested() && rendered < total_target) {
        failed_.store(true, std::memory_order_release);
        spdlog::error("Render job '{}' produced {} samples but expected {}",
                      job_.emitter_id, rendered, total_target);
        if (!first_block) {
            SampleBlock sentinel;
            sentinel.end_of_burst = true;
            sentinel.start_of_burst = false;
            queue_.push_wait(std::move(sentinel), stoken);
        }
    }

    spdlog::info("Render complete: {} samples, target {}", samples_rendered_.load(), total_target);
    finish();
    } catch (const std::exception& e) {
        spdlog::error("Render job '{}' failed: {}", job_.emitter_id, e.what());
        failed_.store(true, std::memory_order_release);
        finish();
    } catch (...) {
        spdlog::error("Render job '{}' failed with an unknown error", job_.emitter_id);
        failed_.store(true, std::memory_order_release);
        finish();
    }
}

std::unique_ptr<dsp::ISource> RenderWorker::create_source(dsp::WaveformType type) {
    return dsp::create_source(type);
}

std::vector<std::complex<float>> RenderWorker::pre_render(const RenderJob& job) {
    try {
    if (job.block_size == 0) return {};
    if (job.block_size > kMaxRenderBlockSamples) return {};
    auto total_target_opt = render_target_samples(job.sample_rate, job.duration_sec);
    if (!total_target_opt.has_value()) return {};
    if (*total_target_opt > kMaxPreRenderSamples) return {};

    std::string type_str = waveform_type_string(job.waveform_config);
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

    size_t total_target = *total_target_opt;
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

    if (rendered != total_target) {
        spdlog::error("Pre-render job '{}' produced {} samples but expected {}",
                      job.emitter_id, rendered, total_target);
        return {};
    }

    return buffer;
    } catch (const std::exception& e) {
        spdlog::error("Pre-render job '{}' failed: {}", job.emitter_id, e.what());
        return {};
    } catch (...) {
        spdlog::error("Pre-render job '{}' failed with an unknown error", job.emitter_id);
        return {};
    }
}

} // namespace archerfish::runtime
