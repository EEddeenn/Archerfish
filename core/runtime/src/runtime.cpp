#include "archerfish/runtime/runtime.hpp"

#include <chrono>
#include <complex>
#include <mutex>
#include <thread>

#include <spdlog/spdlog.h>

#include "archerfish/dsp/source_factory.hpp"
#include "archerfish/dsp/waveform_type.hpp"
#include "archerfish/runtime/event_dispatcher.hpp"

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
            {"type", dsp::to_string(instr.waveform.type)},
        };
        job.waveform_config.update(instr.waveform.params);
        job.sample_rate = instr.sample_rate;
        job.duration_sec = instr.duration_sec;
        job.start_sec = instr.start_sec;
        job.block_size = config_.block_size;
        job.impairments = instr.impairments;
        render_jobs_.push_back(std::move(job));
    }

    mix_group_jobs_.clear();
    for (const auto& mg : current_plan_.mix_groups) {
        MixGroupJob mgj;
        mgj.device_id = mg.device_id;
        mgj.channel = mg.channel;
        mgj.start_sec = mg.start_sec;
        mgj.duration_sec = mg.duration_sec;
        mgj.estimated_peak_sum = mg.estimated_peak_sum;
        for (const auto& eid : mg.emitter_ids) {
            for (const auto& instr : current_plan_.render_instructions) {
                if (instr.emitter_id == eid) {
                    RenderJob job;
                    job.emitter_id = instr.emitter_id;
                    job.waveform_config = nlohmann::json{
                        {"type", dsp::to_string(instr.waveform.type)},
                    };
                    job.waveform_config.update(instr.waveform.params);
                    job.sample_rate = instr.sample_rate;
                    job.duration_sec = instr.duration_sec;
                    job.start_sec = instr.start_sec;
                    job.block_size = config_.block_size;
                    job.impairments = instr.impairments;
                    mgj.member_jobs.push_back(std::move(job));
                    break;
                }
            }
        }
        mix_group_jobs_.push_back(std::move(mgj));
    }

    return true;
}

bool Runtime::run() {
    if (!state_machine_.transition_to(RuntimeState::Running)) {
        return false;
    }

    auto run_start = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point tx_start_time;
    auto stoken = stop_source_.get_token();

    EventDispatcher dispatcher;
    for (const auto& evt : current_plan_.timeline) {
        if (evt.type == scenario::TimelineEventType::FreqChange) {
            double freq = evt.payload.value("freq_hz", 0.0);
            uint32_t ch = evt.payload.value("channel", config_.channel);
            dispatcher.schedule(evt.time_sec, [this, ch, freq]() {
                device_->set_center_freq(ch, freq);
            });
        } else if (evt.type == scenario::TimelineEventType::GainChange) {
            double gain = evt.payload.value("gain_db", 0.0);
            uint32_t ch = evt.payload.value("channel", config_.channel);
            dispatcher.schedule(evt.time_sec, [this, ch, gain]() {
                device_->set_gain(ch, gain);
            });
        } else if (evt.type == scenario::TimelineEventType::Marker) {
            std::string name = evt.payload.value("name", evt.target_id);
            double planned_time = evt.time_sec;
            dispatcher.schedule(evt.time_sec, [this, name, planned_time]() {
                auto now = std::chrono::system_clock::now();
                double wall_sec = std::chrono::duration<double>(now.time_since_epoch()).count();
                spdlog::info("[MARKER] {} @ {}", name, wall_sec);
                MarkerDispatch md;
                md.name = name;
                md.planned_time_sec = planned_time;
                md.wall_clock_sec = wall_sec;
                marker_dispatches_.push_back(std::move(md));
            });
        }
    }
    if (dispatcher.total_events() > 0) {
        dispatcher.start();
    }

    std::unordered_set<std::string> mix_group_emitter_ids;
    for (const auto& mg : current_plan_.mix_groups) {
        for (const auto& eid : mg.emitter_ids) {
            mix_group_emitter_ids.insert(eid);
        }
    }

    std::vector<size_t> solo_job_indices;
    for (size_t i = 0; i < render_jobs_.size(); ++i) {
        if (mix_group_emitter_ids.find(render_jobs_[i].emitter_id) == mix_group_emitter_ids.end()) {
            solo_job_indices.push_back(i);
        }
    }

    for (size_t si = 0; si < solo_job_indices.size(); ++si) {
        if (stoken.stop_requested()) break;

        size_t job_idx = solo_job_indices[si];
        const auto& job = render_jobs_[job_idx];

        if (si > 0) {
            size_t prev_idx = solo_job_indices[si - 1];
            double prev_end = render_jobs_[prev_idx].start_sec + render_jobs_[prev_idx].duration_sec;
            double delay = job.start_sec - prev_end;
            if (delay > 0.0) {
                std::unique_lock lock(abort_mutex_);
                abort_cv_.wait_for(lock, std::chrono::duration<double>(delay),
                                   [this] { return stop_source_.stop_requested(); });
                if (stop_source_.stop_requested()) break;
            }
        } else if (job.start_sec > 0.0) {
            std::unique_lock lock(abort_mutex_);
            abort_cv_.wait_for(lock, std::chrono::duration<double>(job.start_sec),
                               [this] { return stop_source_.stop_requested(); });
            if (stop_source_.stop_requested()) break;
        }

        if (si == 0) {
            device_->start_tx(config_.channel);
            tx_start_time = std::chrono::steady_clock::now();
        }

        queue_ = std::make_unique<SampleQueue>(config_.queue_capacity);
        RenderWorker render_worker(*queue_, job, stop_source_);

        render_worker.start();

        {
            std::unique_lock lock(queue_->cv_mutex_);
            queue_->cv_not_empty_.wait(lock, [&] {
                return queue_->size() >= config_.queue_capacity / 2 || render_worker.is_complete();
            });
        }

        {
            std::lock_guard lock(active_tx_worker_mutex_);
            active_tx_worker_ = std::make_unique<TxWorker>(*queue_, *device_, config_.channel, stop_source_);
        }
        active_tx_worker_->start();

        render_worker.join();
        if (render_worker.samples_rendered() == 0) {
            active_tx_worker_->request_stop();
        }
        active_tx_worker_->join();

        metrics_.total_samples_sent += active_tx_worker_->metrics().samples_sent.load();
        metrics_.total_blocks_sent += active_tx_worker_->metrics().blocks_sent.load();
        metrics_.underruns += active_tx_worker_->metrics().underruns.load();

        {
            std::lock_guard lock(active_tx_worker_mutex_);
            active_tx_worker_.reset();
        }
    }

    for (size_t mi = 0; mi < mix_group_jobs_.size(); ++mi) {
        if (stoken.stop_requested()) break;

        const auto& mgj = mix_group_jobs_[mi];
        double delay = mgj.start_sec;
        if (mi > 0 || !solo_job_indices.empty()) {
            if (!solo_job_indices.empty()) {
                size_t last_solo = solo_job_indices.back();
                double prev_end = render_jobs_[last_solo].start_sec + render_jobs_[last_solo].duration_sec;
                delay = mgj.start_sec - prev_end;
            } else if (mi > 0) {
                double prev_end = mix_group_jobs_[mi - 1].start_sec + mix_group_jobs_[mi - 1].duration_sec;
                delay = mgj.start_sec - prev_end;
            }
            if (delay > 0.0) {
                std::unique_lock lock(abort_mutex_);
                abort_cv_.wait_for(lock, std::chrono::duration<double>(delay),
                                   [this] { return stop_source_.stop_requested(); });
                if (stop_source_.stop_requested()) break;
            }
        } else if (mgj.start_sec > 0.0) {
            std::unique_lock lock(abort_mutex_);
            abort_cv_.wait_for(lock, std::chrono::duration<double>(mgj.start_sec),
                               [this] { return stop_source_.stop_requested(); });
            if (stop_source_.stop_requested()) break;
        }

        if (solo_job_indices.empty() && mi == 0) {
            device_->start_tx(config_.channel);
            tx_start_time = std::chrono::steady_clock::now();
        }

        size_t total_samples = static_cast<size_t>(mgj.member_jobs.front().sample_rate * mgj.duration_sec);
        std::vector<std::complex<float>> mixed_buffer(total_samples, {0.0f, 0.0f});

        for (const auto& member_job : mgj.member_jobs) {
            std::string type_str = member_job.waveform_config.value("type", "");
            auto type_result = dsp::waveform_type_from_string(type_str);
            if (!type_result.has_value()) continue;

            auto source = dsp::create_source(*type_result);
            if (!source) continue;

            auto config = member_job.waveform_config;
            config["sample_rate"] = member_job.sample_rate;
            config["duration_sec"] = mgj.duration_sec;

            source->configure(config);
            source->prepare();

            size_t rendered = 0;
            while (rendered < total_samples) {
                size_t to_render = std::min(total_samples - rendered, config_.block_size);
                std::vector<std::complex<float>> block(to_render);
                size_t produced = source->render_block(block.data(), to_render);
                if (produced == 0) break;
                for (size_t s = 0; s < produced && (rendered + s) < total_samples; ++s) {
                    mixed_buffer[rendered + s] += block[s];
                }
                rendered += produced;
            }
        }

        queue_ = std::make_unique<SampleQueue>(config_.queue_capacity);

        size_t offset = 0;
        while (offset < total_samples) {
            if (stoken.stop_requested()) break;

            size_t block_sz = std::min(total_samples - offset, config_.block_size);
            SampleBlock block;
            block.samples.assign(mixed_buffer.begin() + static_cast<ptrdiff_t>(offset),
                                 mixed_buffer.begin() + static_cast<ptrdiff_t>(offset + block_sz));
            block.start_of_burst = (offset == 0);
            block.end_of_burst = (offset + block_sz >= total_samples);

            if (!queue_->push_wait(std::move(block), stoken)) break;
            offset += block_sz;
        }

        SampleBlock sentinel;
        sentinel.end_of_burst = true;
        sentinel.start_of_burst = false;
        queue_->push_wait(std::move(sentinel), stoken);

        {
            std::lock_guard lock(active_tx_worker_mutex_);
            active_tx_worker_ = std::make_unique<TxWorker>(*queue_, *device_, config_.channel, stop_source_);
        }
        active_tx_worker_->start();
        active_tx_worker_->join();

        metrics_.total_samples_sent += active_tx_worker_->metrics().samples_sent.load();
        metrics_.total_blocks_sent += active_tx_worker_->metrics().blocks_sent.load();
        metrics_.underruns += active_tx_worker_->metrics().underruns.load();

        {
            std::lock_guard lock(active_tx_worker_mutex_);
            active_tx_worker_.reset();
        }
    }

    if (dispatcher.total_events() > 0) {
        dispatcher.wait_complete();
        auto dispatched_markers = dispatcher.marker_dispatches();
        for (const auto& md : dispatched_markers) {
            marker_dispatches_.push_back(md);
        }
    }

    if (!render_jobs_.empty() || !mix_group_jobs_.empty()) {
        double rate = config_.channel < current_plan_.channels.size()
                          ? current_plan_.channels[config_.channel].rf.rate_sps
                          : 1e6;
        double expected_air_time = static_cast<double>(metrics_.total_samples_sent) / rate;
        auto tx_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - tx_start_time).count();
        double drain_remaining = expected_air_time - tx_elapsed;
        if (drain_remaining > 0.0) {
            std::unique_lock lock(abort_mutex_);
            abort_cv_.wait_for(lock, std::chrono::duration<double>(drain_remaining),
                               [this] { return stop_source_.stop_requested(); });
        }
        device_->stop_tx(config_.channel);
    }

    auto run_stop = std::chrono::steady_clock::now();

    auto start_sec = std::chrono::duration<double>(run_start.time_since_epoch()).count();
    auto stop_sec = std::chrono::duration<double>(run_stop.time_since_epoch()).count();

    metrics_.actual_start_sec = start_sec;
    metrics_.actual_stop_sec = stop_sec;
    metrics_.actual_duration_sec = stop_sec - start_sec;

    (void)state_machine_.transition_to(RuntimeState::Completed);
    return true;
}

void Runtime::abort() {
    stop_source_.request_stop();
    {
        std::lock_guard lock(abort_mutex_);
        abort_cv_.notify_all();
    }
    {
        std::lock_guard lock(active_tx_worker_mutex_);
        if (active_tx_worker_) active_tx_worker_->request_stop();
    }
    (void)state_machine_.transition_to(RuntimeState::Aborted);
    device_->stop_tx(config_.channel);
    {
        std::lock_guard lock(active_tx_worker_mutex_);
        if (active_tx_worker_) {
            active_tx_worker_->join();
            active_tx_worker_.reset();
        }
    }
}

RuntimeState Runtime::state() const {
    return state_machine_.current();
}

Runtime::RunMetrics Runtime::get_metrics() const {
    return metrics_;
}

const std::vector<MarkerDispatch>& Runtime::marker_dispatches() const {
    return marker_dispatches_;
}

} // namespace archerfish::runtime
