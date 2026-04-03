#include "archerfish/runtime/tx_worker.hpp"

#include <thread>

namespace archerfish::runtime {

TxWorker::TxWorker(SampleQueue& input_queue, hal::IHalDevice& device, uint32_t channel)
    : queue_(input_queue), device_(device), channel_(channel) {}

void TxWorker::start() {
    metrics_.active.store(true, std::memory_order_release);
    thread_ = std::thread(&TxWorker::run, this);
}

void TxWorker::request_stop() {
    stop_requested_.store(true, std::memory_order_release);
}

void TxWorker::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

const TxMetrics& TxWorker::metrics() const {
    return metrics_;
}

void TxWorker::run() {
    bool got_first_block = false;

    while (true) {
        auto block = queue_.pop();
        if (block.has_value()) {
            hal::TxMetadata meta;
            meta.start_of_burst = block->start_of_burst;
            meta.end_of_burst = block->end_of_burst;
            meta.has_time_spec = block->has_time_spec;
            meta.time_spec_sec = block->time_spec_sec;

            auto sz = block->samples.size();
            if (!block->samples.empty()) {
                device_.send_samples(channel_, block->samples.data(),
                                     sz, meta);
            }

            metrics_.samples_sent.fetch_add(sz, std::memory_order_relaxed);
            metrics_.blocks_sent.fetch_add(1, std::memory_order_relaxed);
            got_first_block = true;

            if (block->end_of_burst) {
                break;
            }
        } else {
            if (stop_requested_.load(std::memory_order_acquire) && queue_.empty()) {
                break;
            }
            if (got_first_block) {
                metrics_.underruns.fetch_add(1, std::memory_order_relaxed);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }

    metrics_.active.store(false, std::memory_order_release);
}

} // namespace archerfish::runtime
