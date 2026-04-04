#include "archerfish/runtime/tx_worker.hpp"

#include <spdlog/spdlog.h>

#include <thread>

namespace archerfish::runtime {

TxWorker::TxWorker(SampleQueue& input_queue, hal::IHalDevice& device, uint32_t channel,
                   std::stop_source stop_src)
    : queue_(input_queue), device_(device), channel_(channel), stop_source_(std::move(stop_src)) {}

void TxWorker::start() {
    metrics_.active.store(true, std::memory_order_release);
    thread_ = std::thread(&TxWorker::run, this);
}

void TxWorker::request_stop() {
    stop_source_.request_stop();
    queue_.notify_all();
}

void TxWorker::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

const TxMetrics& TxWorker::metrics() const {
    return metrics_;
}

std::stop_token TxWorker::get_stop_token() const {
    return stop_source_.get_token();
}

void TxWorker::run() {
    auto stoken = stop_source_.get_token();

    while (true) {
        auto block = queue_.pop_wait(stoken);
        if (!block.has_value()) break;

        hal::TxMetadata meta;
        meta.start_of_burst = block->start_of_burst;
        meta.end_of_burst = block->end_of_burst;
        meta.has_time_spec = block->has_time_spec;
        meta.time_spec_sec = block->time_spec_sec;

        auto sz = block->samples.size();
        size_t actually_sent = 0;
        if (!block->samples.empty()) {
            actually_sent = device_.send_samples(channel_, block->samples.data(),
                                 sz, meta);
        }

        metrics_.samples_sent.fetch_add(actually_sent, std::memory_order_relaxed);
        metrics_.blocks_sent.fetch_add(1, std::memory_order_relaxed);

        if (block->end_of_burst) {
            break;
        }
    }

    metrics_.active.store(false, std::memory_order_release);
}

} // namespace archerfish::runtime
