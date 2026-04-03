#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

#include "archerfish/hal/hal_device.hpp"
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
    };
    [[nodiscard]] RunMetrics get_metrics() const;

private:
    std::shared_ptr<hal::IHalDevice> device_;
    RuntimeConfig config_;
    StateMachine state_machine_;
    std::unique_ptr<SampleQueue> queue_;
    std::vector<RenderJob> render_jobs_;
    std::unique_ptr<TxWorker> active_tx_worker_;
    std::mutex active_tx_worker_mutex_;
    RunMetrics metrics_;
    scenario::Plan current_plan_;
};

} // namespace archerfish::runtime
