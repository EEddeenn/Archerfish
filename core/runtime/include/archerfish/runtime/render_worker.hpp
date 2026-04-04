#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "archerfish/dsp/source.hpp"
#include "archerfish/runtime/spsc_queue.hpp"
#include "archerfish/scenario/scenario.hpp"

namespace archerfish::runtime {

struct RenderJob {
    std::string emitter_id;
    nlohmann::json waveform_config;
    double sample_rate{0.0};
    double duration_sec{0.0};
    double start_sec{0.0};
    size_t block_size{32768};
    std::optional<scenario::ImpairmentSettings> impairments;
};

class RenderWorker {
public:
    RenderWorker(SampleQueue& output_queue, const RenderJob& job,
                 std::stop_source stop_src = std::stop_source());

    void start();
    void join();
    void request_stop();

    [[nodiscard]] bool is_complete() const;
    [[nodiscard]] size_t samples_rendered() const;

private:
    void run();
    std::unique_ptr<dsp::ISource> create_source(dsp::WaveformType type);

    SampleQueue& queue_;
    RenderJob job_;
    std::thread thread_;
    std::stop_source stop_source_;
    std::atomic<bool> complete_{false};
    std::atomic<size_t> samples_rendered_{0};
};

} // namespace archerfish::runtime
