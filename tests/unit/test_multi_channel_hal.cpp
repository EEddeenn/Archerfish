#include <catch2/catch_test_macros.hpp>

#include "archerfish/hal/stub_device.hpp"

using namespace archerfish::hal;

TEST_CASE("StubDevice reports multi-channel capabilities", "[hal][multi_channel]") {
    DeviceCapabilities caps;
    caps.num_channels = 2;
    StubDevice device("stub0", caps);

    auto reported = device.get_capabilities();
    REQUIRE(reported.num_channels == 2);
}

TEST_CASE("StubDevice configures RF independently per channel", "[hal][multi_channel]") {
    StubDevice device("stub0");

    device.set_center_freq(0, 1e9);
    device.set_sample_rate(0, 10e6);
    device.set_gain(0, 20.0);

    device.set_center_freq(1, 2.4e9);
    device.set_sample_rate(1, 20e6);
    device.set_gain(1, 30.0);

    const auto& history = device.call_history();
    int ch0_freq_count = 0, ch1_freq_count = 0;
    int ch0_rate_count = 0, ch1_rate_count = 0;
    int ch0_gain_count = 0, ch1_gain_count = 0;

    for (const auto& call : history) {
        if (call.method == "set_center_freq") {
            if (call.channel == 0 && call.value == 1e9) ch0_freq_count++;
            if (call.channel == 1 && call.value == 2.4e9) ch1_freq_count++;
        }
        if (call.method == "set_sample_rate") {
            if (call.channel == 0 && call.value == 10e6) ch0_rate_count++;
            if (call.channel == 1 && call.value == 20e6) ch1_rate_count++;
        }
        if (call.method == "set_gain") {
            if (call.channel == 0 && call.value == 20.0) ch0_gain_count++;
            if (call.channel == 1 && call.value == 30.0) ch1_gain_count++;
        }
    }

    REQUIRE(ch0_freq_count == 1);
    REQUIRE(ch1_freq_count == 1);
    REQUIRE(ch0_rate_count == 1);
    REQUIRE(ch1_rate_count == 1);
    REQUIRE(ch0_gain_count == 1);
    REQUIRE(ch1_gain_count == 1);
}

TEST_CASE("StubDevice start/stop TX independently per channel", "[hal][multi_channel]") {
    StubDevice device("stub0");

    REQUIRE_FALSE(device.is_tx_active(0));
    REQUIRE_FALSE(device.is_tx_active(1));

    device.start_tx(0);
    REQUIRE(device.is_tx_active(0));
    REQUIRE_FALSE(device.is_tx_active(1));

    device.start_tx(1);
    REQUIRE(device.is_tx_active(0));
    REQUIRE(device.is_tx_active(1));

    device.stop_tx(0);
    REQUIRE_FALSE(device.is_tx_active(0));
    REQUIRE(device.is_tx_active(1));

    device.stop_tx(1);
    REQUIRE_FALSE(device.is_tx_active(0));
    REQUIRE_FALSE(device.is_tx_active(1));
}

TEST_CASE("StubDevice send_samples per channel", "[hal][multi_channel]") {
    StubDevice device("stub0");

    device.start_tx(0);
    device.start_tx(1);

    std::vector<std::complex<float>> samples(100, {0.5f, 0.0f});
    TxMetadata meta;

    (void)device.send_samples(0, samples.data(), 100, meta);
    (void)device.send_samples(0, samples.data(), 50, meta);
    (void)device.send_samples(1, samples.data(), 200, meta);

    REQUIRE(device.total_samples_sent(0) == 150);
    REQUIRE(device.total_samples_sent(1) == 200);
    REQUIRE(device.total_samples_sent(2) == 0);
}

TEST_CASE("StubDevice send_samples rejects when TX not active on channel", "[hal][multi_channel]") {
    StubDevice device("stub0");

    std::vector<std::complex<float>> samples(100, {0.5f, 0.0f});
    TxMetadata meta;

    (void)device.send_samples(0, samples.data(), 100, meta);
    REQUIRE(device.total_samples_sent(0) == 0);
}

TEST_CASE("StubDevice bandwidth and antenna per channel", "[hal][multi_channel]") {
    StubDevice device("stub0");

    device.set_bandwidth(0, 8e6);
    device.set_bandwidth(1, 16e6);
    device.set_antenna(0, "TX/RX");
    device.set_antenna(1, "TX2");

    const auto& history = device.call_history();
    bool found_bw0 = false, found_bw1 = false;
    bool found_ant0 = false, found_ant1 = false;

    for (const auto& call : history) {
        if (call.method == "set_bandwidth") {
            if (call.channel == 0 && call.value == 8e6) found_bw0 = true;
            if (call.channel == 1 && call.value == 16e6) found_bw1 = true;
        }
        if (call.method == "set_antenna") {
            if (call.channel == 0 && call.str_value == "TX/RX") found_ant0 = true;
            if (call.channel == 1 && call.str_value == "TX2") found_ant1 = true;
        }
    }

    REQUIRE(found_bw0);
    REQUIRE(found_bw1);
    REQUIRE(found_ant0);
    REQUIRE(found_ant1);
}

TEST_CASE("StubDevice reset clears per-channel state", "[hal][multi_channel]") {
    StubDevice device("stub0");

    device.start_tx(0);
    device.start_tx(1);

    std::vector<std::complex<float>> samples(100, {0.5f, 0.0f});
    TxMetadata meta;
    (void)device.send_samples(0, samples.data(), 100, meta);
    (void)device.send_samples(1, samples.data(), 200, meta);

    REQUIRE(device.total_samples_sent(0) == 100);
    REQUIRE(device.total_samples_sent(1) == 200);

    device.reset();

    REQUIRE(device.total_samples_sent(0) == 0);
    REQUIRE(device.total_samples_sent(1) == 0);
    REQUIRE_FALSE(device.is_tx_active(0));
    REQUIRE_FALSE(device.is_tx_active(1));
    REQUIRE(device.call_history().empty());
}

TEST_CASE("StubDevice default capabilities single channel", "[hal][multi_channel]") {
    StubDevice device("stub0");

    auto caps = device.get_capabilities();
    REQUIRE(caps.num_channels == 1);
}

TEST_CASE("StubDevice call history tracks channel indices", "[hal][multi_channel]") {
    StubDevice device("stub0");

    device.set_center_freq(0, 1e9);
    device.set_center_freq(1, 2.4e9);
    device.set_gain(0, 10.0);
    device.set_gain(1, 20.0);

    const auto& history = device.call_history();

    REQUIRE(history.size() == 4);
    REQUIRE(history[0].channel == 0);
    REQUIRE(history[0].method == "set_center_freq");
    REQUIRE(history[1].channel == 1);
    REQUIRE(history[1].method == "set_center_freq");
    REQUIRE(history[2].channel == 0);
    REQUIRE(history[2].method == "set_gain");
    REQUIRE(history[3].channel == 1);
    REQUIRE(history[3].method == "set_gain");
}
