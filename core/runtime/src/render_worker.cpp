#include "archerfish/runtime/render_worker.hpp"

#include <algorithm>
#include <thread>

#include <spdlog/spdlog.h>

#include "archerfish/dsp/source_factory.hpp"

#include "archerfish/impairments/impairment_chain.hpp"
#include "archerfish/impairments/cfo.hpp"
#include "archerfish/impairments/awgn.hpp"
#include "archerfish/impairments/phase_offset.hpp"
#include "archerfish/impairments/dc_offset.hpp"
#include "archerfish/impairments/iq_imbalance.hpp"
#include "archerfish/impairments/amplitude_ripple.hpp"
#include "archerfish/impairments/delay.hpp"
#include "archerfish/impairments/burst_dropout.hpp"

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

    // Build impairment chain once per job
    std::optional<impairments::ImpairmentChain> chain;
    if (job_.impairments.has_value()) {
        auto& imp = *job_.impairments;
        impairments::ImpairmentChain c;
        if (imp.cfo_hz.has_value())
            c.add(std::make_unique<impairments::CfoImpairment>(*imp.cfo_hz, job_.sample_rate));
        if (imp.awgn_power.has_value())
            c.add(std::make_unique<impairments::AwgnImpairment>(*imp.awgn_power));
        if (imp.phase_offset_rad.has_value())
            c.add(std::make_unique<impairments::PhaseOffsetImpairment>(*imp.phase_offset_rad));
        if (imp.dc_offset_i.has_value() || imp.dc_offset_q.has_value())
            c.add(std::make_unique<impairments::DcOffsetImpairment>(
                imp.dc_offset_i.value_or(0.0), imp.dc_offset_q.value_or(0.0)));
        if (imp.iq_gain_imbalance_db.has_value() || imp.iq_phase_imbalance_rad.has_value())
            c.add(std::make_unique<impairments::IqImbalanceImpairment>(
                imp.iq_gain_imbalance_db.value_or(0.0), imp.iq_phase_imbalance_rad.value_or(0.0)));
        if (imp.amplitude_ripple_db.has_value())
            c.add(std::make_unique<impairments::AmplitudeRippleImpairment>(
                *imp.amplitude_ripple_db, imp.amplitude_ripple_freq_hz.value_or(1000.0), job_.sample_rate));
        if (imp.delay_sec.has_value())
            c.add(std::make_unique<impairments::DelayImpairment>(*imp.delay_sec, job_.sample_rate));
        if (imp.burst_dropout_rate.has_value())
            c.add(std::make_unique<impairments::BurstDropoutImpairment>(*imp.burst_dropout_rate));
        if (c.size() > 0)
            chain.emplace(std::move(c));
    }

    size_t total_target = static_cast<size_t>(job_.sample_rate * job_.duration_sec);
    bool first_block = true;

    while (samples_rendered_.load(std::memory_order_relaxed) < total_target) {
        size_t remaining = total_target - samples_rendered_.load(std::memory_order_relaxed);
        size_t to_render = std::min(remaining, job_.block_size);

        SampleBlock block;
        block.samples.resize(to_render);

        size_t produced = source->render_block(block.samples.data(), to_render);
        if (produced == 0) break;

        if (chain.has_value()) {
            chain->apply(block.samples.data(), produced);
        }

        block.samples.resize(produced);
        block.start_of_burst = first_block;
        first_block = false;

        size_t new_total = samples_rendered_.load(std::memory_order_relaxed) + produced;
        if (new_total >= total_target) {
            block.end_of_burst = true;
        }

        while (!queue_.push(std::move(block))) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        samples_rendered_.store(new_total, std::memory_order_release);
    }

    if (samples_rendered_.load(std::memory_order_relaxed) < total_target && !first_block) {
        SampleBlock sentinel;
        sentinel.end_of_burst = true;
        sentinel.start_of_burst = false;
        while (!queue_.push(std::move(sentinel))) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    spdlog::info("Render complete: {} samples, target {}", samples_rendered_.load(), total_target);
    complete_.store(true, std::memory_order_release);
}

std::unique_ptr<dsp::ISource> RenderWorker::create_source(const std::string& type) {
    return dsp::create_source(type);
}

} // namespace archerfish::runtime
