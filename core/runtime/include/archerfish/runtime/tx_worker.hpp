#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

#include "archerfish/hal/hal_device.hpp"
#include "archerfish/runtime/spsc_queue.hpp"

namespace archerfish::runtime {

struct TxMetrics {
    std::atomic<size_t> samples_sent{0};
    std::atomic<size_t> blocks_sent{0};
    std::atomic<size_t> underruns{0};
    std::atomic<bool> active{false};
};

class TxWorker {
public:
    TxWorker(SampleQueue& input_queue, hal::IHalDevice& device, uint32_t channel);

    void start();
    void request_stop();
    void join();

    [[nodiscard]] const TxMetrics& metrics() const;

private:
    void run();
    SampleQueue& queue_;
    hal::IHalDevice& device_;
    uint32_t channel_;
    TxMetrics metrics_;
    std::thread thread_;
    std::atomic<bool> stop_requested_{false};
};

} // namespace archerfish::runtime
