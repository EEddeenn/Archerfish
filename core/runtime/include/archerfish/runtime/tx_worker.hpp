#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stop_token>
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
    TxWorker(SampleQueue& input_queue, hal::IHalDevice& device, uint32_t channel,
             std::stop_source stop_src = std::stop_source());

    void start();
    void request_stop();
    void join();

    [[nodiscard]] const TxMetrics& metrics() const;
    [[nodiscard]] std::stop_token get_stop_token() const;

private:
    void run();
    SampleQueue& queue_;
    hal::IHalDevice& device_;
    uint32_t channel_;
    TxMetrics metrics_;
    std::thread thread_;
    std::stop_source stop_source_;
};

} // namespace archerfish::runtime
