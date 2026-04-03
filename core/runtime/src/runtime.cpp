#include "archerfish/runtime/runtime.hpp"

#include <chrono>
#include <thread>

namespace archerfish::runtime {

Runtime::Runtime(std::shared_ptr<hal::IHalDevice> device, RuntimeConfig config)
    : device_(std::move(device)), config_(config) {}

bool Runtime::prepare(const scenario::Plan& plan) {
    if (!state_machine_.transition_to(RuntimeState::Validated)) return false;
    if (!state_machine_.transition_to(RuntimeState::Planned)) return false;
    if (!state_machine_.transition_to(RuntimeState::Prepared)) return false;

    current_plan_ = plan;

    queue_ = std::make_unique<SampleQueue>(config_.queue_capacity);

    for (const auto& ch : plan.channels) {
        device_->set_center_freq(ch.channel_index, ch.rf.freq_hz);
        device_->set_sample_rate(ch.channel_index, ch.rf.rate_sps);
        device_->set_gain(ch.channel_index, ch.rf.gain_db);
        if (ch.rf.bandwidth_hz.has_value()) {
            device_->set_bandwidth(ch.channel_index, *ch.rf.bandwidth_hz);
        }
        if (ch.rf.antenna.has_value()) {
            device_->set_antenna(ch.channel_index, *ch.rf.antenna);
        }
    }

    return true;
}

bool Runtime::arm() {
    if (!state_machine_.transition_to(RuntimeState::Armed)) {
        return false;
    }

    if (current_plan_.render_instructions.empty()) {
        return false;
    }

    render_jobs_.clear();
    for (const auto& instr : current_plan_.render_instructions) {
        RenderJob job;
        job.emitter_id = instr.emitter_id;
        job.waveform_config = nlohmann::json{
            {"type", instr.waveform.type},
        };
        job.waveform_config.update(instr.waveform.params);
        job.sample_rate = instr.sample_rate;
        job.duration_sec = instr.duration_sec;
        job.start_sec = instr.start_sec;
        job.block_size = config_.block_size;
        render_jobs_.push_back(std::move(job));
    }

    return true;
}

bool Runtime::run() {
    if (!state_machine_.transition_to(RuntimeState::Running)) {
        return false;
    }

    auto run_start = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point tx_start_time;

    for (size_t job_idx = 0; job_idx < render_jobs_.size(); ++job_idx) {
        const auto& job = render_jobs_[job_idx];

        if (job_idx > 0) {
            double prev_end = render_jobs_[job_idx - 1].start_sec + render_jobs_[job_idx - 1].duration_sec;
            double delay = job.start_sec - prev_end;
            if (delay > 0.0) {
                std::this_thread::sleep_for(std::chrono::duration<double>(delay));
            }
        } else if (job.start_sec > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(job.start_sec));
        }

        if (job_idx == 0) {
            device_->start_tx(config_.channel);
            tx_start_time = std::chrono::steady_clock::now();
        }

        queue_ = std::make_unique<SampleQueue>(config_.queue_capacity);
        RenderWorker render_worker(*queue_, job);

        render_worker.start();

        while (queue_->size() < config_.queue_capacity / 2 && !render_worker.is_complete()) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        TxWorker tx_worker(*queue_, *device_, config_.channel);
        active_tx_worker_ = &tx_worker;
        tx_worker.start();

        render_worker.join();
        if (render_worker.samples_rendered() == 0) {
            tx_worker.request_stop();
        }
        tx_worker.join();
        active_tx_worker_ = nullptr;

        metrics_.total_samples_sent += tx_worker.metrics().samples_sent.load();
        metrics_.total_blocks_sent += tx_worker.metrics().blocks_sent.load();
        metrics_.underruns += tx_worker.metrics().underruns.load();
    }

    if (!render_jobs_.empty()) {
        double expected_air_time = static_cast<double>(metrics_.total_samples_sent) / render_jobs_.front().sample_rate;
        auto tx_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - tx_start_time).count();
        double drain_remaining = expected_air_time - tx_elapsed;
        if (drain_remaining > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(drain_remaining));
        }
        device_->stop_tx(config_.channel);
    }

    auto run_stop = std::chrono::steady_clock::now();

    auto start_sec = std::chrono::duration<double>(run_start.time_since_epoch()).count();
    auto stop_sec = std::chrono::duration<double>(run_stop.time_since_epoch()).count();

    metrics_.actual_start_sec = start_sec;
    metrics_.actual_stop_sec = stop_sec;
    metrics_.actual_duration_sec = stop_sec - start_sec;

    state_machine_.transition_to(RuntimeState::Completed);
    return true;
}

void Runtime::abort() {
    if (active_tx_worker_) {
        active_tx_worker_->request_stop();
    }
    state_machine_.transition_to(RuntimeState::Aborted);
    device_->stop_tx(config_.channel);
    if (active_tx_worker_) {
        active_tx_worker_->join();
        active_tx_worker_ = nullptr;
    }
}

RuntimeState Runtime::state() const {
    return state_machine_.current();
}

Runtime::RunMetrics Runtime::get_metrics() const {
    return metrics_;
}

} // namespace archerfish::runtime
