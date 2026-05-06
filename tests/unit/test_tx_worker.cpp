#include <catch2/catch_test_macros.hpp>

#include "archerfish/hal/stub_device.hpp"
#include "archerfish/runtime/tx_worker.hpp"

#include <limits>
#include <stdexcept>

using namespace archerfish::runtime;
using namespace archerfish::hal;

namespace {

class ThrowingSendDevice : public StubDevice {
public:
    using StubDevice::StubDevice;

    size_t send_samples(uint32_t channel,
                        const std::complex<float>* data,
                        size_t count,
                        const TxMetadata& meta) override {
        (void)channel;
        (void)data;
        (void)count;
        (void)meta;
        throw std::runtime_error("send failed");
    }
};

class PartialSendDevice : public StubDevice {
public:
    using StubDevice::StubDevice;

    size_t send_samples(uint32_t channel,
                        const std::complex<float>* data,
                        size_t count,
                        const TxMetadata& meta) override {
        const size_t accepted = count / 2;
        return StubDevice::send_samples(channel, data, accepted, meta);
    }
};

} // namespace

TEST_CASE("TxWorker sends blocks through queue to device", "[runtime][tx]") {
    StubDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    TxWorker worker(queue, device, 0);
    worker.start();

    SampleBlock block1;
    block1.samples = {{1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}};
    block1.start_of_burst = true;
    REQUIRE(queue.push_notify(std::move(block1)));

    SampleBlock block2;
    block2.samples = {{0.5f, 0.5f}, {-0.5f, -0.5f}};
    block2.end_of_burst = true;
    REQUIRE(queue.push_notify(std::move(block2)));

    worker.join();

    REQUIRE(device.total_samples_sent(0) == 5);
    REQUIRE(worker.metrics().samples_sent.load() == 5);
    REQUIRE(worker.metrics().blocks_sent.load() == 2);
}

TEST_CASE("TxWorker total samples_sent matches expected", "[runtime][tx]") {
    StubDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    TxWorker worker(queue, device, 0);
    worker.start();

    size_t total = 0;
    for (int i = 0; i < 3; ++i) {
        SampleBlock block;
        block.samples.resize(100);
        total += 100;
        if (i == 0) block.start_of_burst = true;
        if (i == 2) block.end_of_burst = true;
        REQUIRE(queue.push_notify(std::move(block)));
    }

    worker.join();
    REQUIRE(worker.metrics().samples_sent.load() == total);
    REQUIRE(device.total_samples_sent(0) == total);
}

TEST_CASE("TxWorker start_of_burst and end_of_burst metadata", "[runtime][tx]") {
    StubDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    TxWorker worker(queue, device, 0);
    worker.start();

    SampleBlock first;
    first.samples = {{1.0f, 0.0f}};
    first.start_of_burst = true;
    REQUIRE(queue.push_notify(std::move(first)));

    SampleBlock last;
    last.samples = {{0.0f, 1.0f}};
    last.end_of_burst = true;
    REQUIRE(queue.push_notify(std::move(last)));

    worker.join();

    const auto& history = device.call_history();
    bool found_sob = false;
    bool found_eob = false;
    for (const auto& call : history) {
        if (call.method == "send_samples") {
            if (call.metadata.start_of_burst) found_sob = true;
            if (call.metadata.end_of_burst) found_eob = true;
        }
    }
    REQUIRE(found_sob);
    REQUIRE(found_eob);
}

TEST_CASE("TxWorker graceful shutdown via request_stop", "[runtime][tx]") {
    StubDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    TxWorker worker(queue, device, 0);
    worker.start();

    SampleBlock block;
    block.samples = {{1.0f, 0.0f}};
    block.start_of_burst = true;
    block.end_of_burst = true;
    REQUIRE(queue.push_notify(std::move(block)));

    worker.request_stop();
    worker.join();

    REQUIRE_FALSE(worker.metrics().active.load());
}

TEST_CASE("TxWorker metrics are accessible", "[runtime][tx]") {
    StubDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    TxWorker worker(queue, device, 0);
    worker.start();

    SampleBlock block;
    block.samples = {{1.0f, 0.0f}, {2.0f, 0.0f}};
    block.start_of_burst = true;
    block.end_of_burst = true;
    REQUIRE(queue.push_notify(std::move(block)));

    worker.join();

    const auto& m = worker.metrics();
    REQUIRE(m.samples_sent.load() == 2);
    REQUIRE(m.blocks_sent.load() == 1);
}

TEST_CASE("TxWorker handles device send exceptions", "[runtime][tx]") {
    ThrowingSendDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    TxWorker worker(queue, device, 0);
    worker.start();

    SampleBlock block;
    block.samples = {{1.0f, 0.0f}};
    block.start_of_burst = true;
    block.end_of_burst = true;
    REQUIRE(queue.push_notify(std::move(block)));

    worker.join();

    REQUIRE_FALSE(worker.metrics().active.load());
    REQUIRE(worker.metrics().failed.load());
    REQUIRE(worker.metrics().samples_sent.load() == 0);
    REQUIRE(worker.metrics().blocks_sent.load() == 0);
}

TEST_CASE("TxWorker records underrun on partial device send", "[runtime][tx]") {
    PartialSendDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    TxWorker worker(queue, device, 0);
    worker.start();

    SampleBlock block;
    block.samples.resize(10);
    block.start_of_burst = true;
    block.end_of_burst = true;
    REQUIRE(queue.push_notify(std::move(block)));

    worker.join();

    REQUIRE_FALSE(worker.metrics().active.load());
    REQUIRE(worker.metrics().samples_sent.load() == 5);
    REQUIRE(worker.metrics().blocks_sent.load() == 1);
    REQUIRE(worker.metrics().underruns.load() == 1);
    REQUIRE(worker.metrics().failed.load());
    REQUIRE(device.total_samples_sent(0) == 5);
}

TEST_CASE("TxWorker rejects non-finite timed send metadata", "[runtime][tx]") {
    StubDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    TxWorker worker(queue, device, 0);
    worker.start();

    SampleBlock block;
    block.samples = {{1.0f, 0.0f}};
    block.start_of_burst = true;
    block.end_of_burst = true;
    block.has_time_spec = true;
    block.time_spec_sec = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(queue.push_notify(std::move(block)));

    worker.join();

    REQUIRE_FALSE(worker.metrics().active.load());
    REQUIRE(worker.metrics().samples_sent.load() == 0);
    REQUIRE(worker.metrics().blocks_sent.load() == 0);
    REQUIRE(worker.metrics().underruns.load() == 1);
    REQUIRE(worker.metrics().failed.load());
    REQUIRE(device.total_samples_sent(0) == 0);
}

TEST_CASE("TxWorker destructor stops and joins running worker", "[runtime][tx]") {
    StubDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    {
        TxWorker worker(queue, device, 0);
        worker.start();
    }

    SUCCEED("TxWorker destructor returned without std::terminate");
}

TEST_CASE("TxWorker rejects double start", "[runtime][tx]") {
    StubDevice device;
    device.start_tx(0);
    SampleQueue queue(16);

    TxWorker worker(queue, device, 0);
    worker.start();
    CHECK_THROWS_AS(worker.start(), std::logic_error);
    worker.request_stop();
    worker.join();
}
