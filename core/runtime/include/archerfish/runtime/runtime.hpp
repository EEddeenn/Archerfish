#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "archerfish/hal/hal_device.hpp"
#include "archerfish/runtime/event_dispatcher.hpp"
#include "archerfish/runtime/render_worker.hpp"
#include "archerfish/runtime/spsc_queue.hpp"
#include "archerfish/runtime/state.hpp"
#include "archerfish/runtime/tx_worker.hpp"
#include "archerfish/scenario/plan.hpp"

namespace archerfish::runtime {

struct RuntimeConfig {
    size_t queue_capacity{64};
    size_t block_size{32768};
    uint32_t channel{0};
    scenario::RunMode run_mode{scenario::RunMode::Realtime};
};

struct ChannelMetrics {
    uint32_t channel_index{0};
    size_t samples_sent{0};
    size_t blocks_sent{0};
    size_t underruns{0};
    double active_duration_sec{0.0};
};

struct MixGroupJob {
    std::string device_id;
    uint32_t channel{0};
    double start_sec{0.0};
    double duration_sec{0.0};
    double estimated_peak_sum{0.0};
    std::vector<RenderJob> member_jobs;
};

class Runtime {
public:
    explicit Runtime(std::shared_ptr<hal::IHalDevice> device, RuntimeConfig config = {});

    [[nodiscard]] bool prepare(const scenario::Plan& plan);
    [[nodiscard]] bool arm();
    [[nodiscard]] bool run();
    void abort();

    [[nodiscard]] RuntimeState state() const;

    struct RunMetrics {
        size_t total_samples_sent{0};
        size_t total_blocks_sent{0};
        size_t underruns{0};
        double actual_start_sec{0.0};
        double actual_stop_sec{0.0};
        double actual_duration_sec{0.0};
        std::vector<ChannelMetrics> per_channel;
    };
    [[nodiscard]] RunMetrics get_metrics() const;
    [[nodiscard]] const std::vector<MarkerDispatch>& marker_dispatches() const;
    [[nodiscard]] const std::vector<WaveformSwitchDispatch>& waveform_switch_dispatches() const;
    [[nodiscard]] const std::vector<ImpairmentChangeDispatch>& impairment_change_dispatches() const;

private:
    struct ChannelExecutor {
        uint32_t channel_index{0};
        std::unique_ptr<SampleQueue> queue;
        std::vector<RenderJob> render_jobs;
        std::vector<MixGroupJob> mix_group_jobs;
        std::unique_ptr<TxWorker> tx_worker;
        std::unique_ptr<RenderWorker> render_worker;
        ChannelMetrics metrics;
    };

    static std::vector<uint32_t> get_active_channels(const scenario::Plan& plan);
    bool run_single_channel();
    bool run_replay();
    bool run_multi_channel(const std::vector<uint32_t>& channels);
    void execute_channel_jobs(ChannelExecutor& exec, std::stop_token stoken);

    std::shared_ptr<hal::IHalDevice> device_;
    RuntimeConfig config_;
    StateMachine state_machine_;
    std::unique_ptr<SampleQueue> queue_;
    std::vector<RenderJob> render_jobs_;
    std::vector<MixGroupJob> mix_group_jobs_;
    std::unique_ptr<TxWorker> active_tx_worker_;
    std::mutex active_tx_worker_mutex_;
    RunMetrics metrics_;
    scenario::Plan current_plan_;
    std::stop_source stop_source_;
    std::mutex abort_mutex_;
    std::condition_variable abort_cv_;
    std::vector<MarkerDispatch> marker_dispatches_;
    std::vector<WaveformSwitchDispatch> waveform_switch_dispatches_;
    std::vector<ImpairmentChangeDispatch> impairment_change_dispatches_;
};

} // namespace archerfish::runtime
