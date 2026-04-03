#include <archerfish/hal/stub_device.hpp>

#include <catch2/catch_test_macros.hpp>
#include <complex>
#include <vector>

using namespace archerfish::hal;

TEST_CASE("StubDevice default construction", "[hal][stub]") {
    StubDevice dev;
    CHECK(dev.device_id() == "stub0");
    auto caps = dev.get_capabilities();
    CHECK(caps.num_channels == 1);
    CHECK(caps.freq_range.min_val == 0.0);
    CHECK(caps.freq_range.max_val == 6e9);
    CHECK(caps.supports_replay == false);
}

TEST_CASE("StubDevice custom construction", "[hal][stub]") {
    DeviceCapabilities custom{};
    custom.num_channels = 2;
    custom.freq_range = {1e6, 3e9};
    custom.supports_replay = true;
    StubDevice dev("custom0", custom);
    CHECK(dev.device_id() == "custom0");
    CHECK(dev.get_capabilities().num_channels == 2);
    CHECK(dev.get_capabilities().supports_replay == true);
}

TEST_CASE("StubDevice RF setters record calls", "[hal][stub]") {
    StubDevice dev;
    dev.set_center_freq(0, 2.4e9);
    dev.set_sample_rate(0, 10e6);
    dev.set_bandwidth(0, 8e6);
    dev.set_gain(0, 20.0);
    dev.set_antenna(0, "TX/RX");

    auto& hist = dev.call_history();
    REQUIRE(hist.size() == 5);

    CHECK(hist[0].method == "set_center_freq");
    CHECK(hist[0].channel == 0);
    CHECK(hist[0].value == 2.4e9);

    CHECK(hist[1].method == "set_sample_rate");
    CHECK(hist[1].value == 10e6);

    CHECK(hist[2].method == "set_bandwidth");
    CHECK(hist[2].value == 8e6);

    CHECK(hist[3].method == "set_gain");
    CHECK(hist[3].value == 20.0);

    CHECK(hist[4].method == "set_antenna");
    CHECK(hist[4].str_value == "TX/RX");
}

TEST_CASE("StubDevice clock/time control records calls", "[hal][stub]") {
    StubDevice dev;
    dev.set_clock_source("external");
    dev.set_time_source("gpsdo");
    dev.sync_time_now();

    auto& hist = dev.call_history();
    REQUIRE(hist.size() == 3);
    CHECK(hist[0].method == "set_clock_source");
    CHECK(hist[0].str_value == "external");
    CHECK(hist[1].method == "set_time_source");
    CHECK(hist[1].str_value == "gpsdo");
    CHECK(hist[2].method == "sync_time_now");
}

TEST_CASE("StubDevice start_tx/stop_tx track state", "[hal][stub]") {
    StubDevice dev;
    CHECK_FALSE(dev.is_tx_active(0));

    dev.start_tx(0);
    CHECK(dev.is_tx_active(0));

    dev.stop_tx(0);
    CHECK_FALSE(dev.is_tx_active(0));
}

TEST_CASE("StubDevice send_samples counts samples", "[hal][stub]") {
    StubDevice dev;
    std::vector<std::complex<float>> samples(100, {1.0f, 0.0f});

    TxMetadata meta{.time_spec_sec = 1.5, .has_time_spec = true, .start_of_burst = true, .end_of_burst = false};
    dev.start_tx(0);
    dev.send_samples(0, samples.data(), 100, meta);

    CHECK(dev.total_samples_sent(0) == 100);

    auto& hist = dev.call_history();
    REQUIRE(hist.size() == 2);
    CHECK(hist[1].method == "send_samples");
    CHECK(hist[1].sample_count == 100);
    CHECK(hist[1].metadata.time_spec_sec == 1.5);
    CHECK(hist[1].metadata.has_time_spec == true);
    CHECK(hist[1].metadata.start_of_burst == true);
    CHECK(hist[1].metadata.end_of_burst == false);
}

TEST_CASE("StubDevice total_samples_sent accumulates", "[hal][stub]") {
    StubDevice dev;
    std::vector<std::complex<float>> buf(50, {0.0f, 0.0f});

    dev.start_tx(0);
    dev.send_samples(0, buf.data(), 50, {});
    dev.send_samples(0, buf.data(), 50, {});
    dev.send_samples(0, buf.data(), 25, {});

    CHECK(dev.total_samples_sent(0) == 125);
}

TEST_CASE("StubDevice reset clears history", "[hal][stub]") {
    StubDevice dev;
    dev.start_tx(0);
    dev.set_center_freq(0, 1e9);
    std::vector<std::complex<float>> buf(10);
    dev.send_samples(0, buf.data(), 10, {});

    CHECK_FALSE(dev.call_history().empty());
    CHECK(dev.total_samples_sent(0) == 10);
    CHECK(dev.is_tx_active(0));

    dev.reset();

    CHECK(dev.call_history().empty());
    CHECK(dev.total_samples_sent(0) == 0);
    CHECK_FALSE(dev.is_tx_active(0));
}

TEST_CASE("StubDevice multi-channel isolation", "[hal][stub]") {
    StubDevice dev;
    dev.start_tx(0);
    dev.start_tx(1);

    CHECK(dev.is_tx_active(0));
    CHECK(dev.is_tx_active(1));

    std::vector<std::complex<float>> buf(10);
    dev.send_samples(0, buf.data(), 10, {});
    dev.send_samples(1, buf.data(), 20, {});

    CHECK(dev.total_samples_sent(0) == 10);
    CHECK(dev.total_samples_sent(1) == 20);

    dev.stop_tx(0);
    CHECK_FALSE(dev.is_tx_active(0));
    CHECK(dev.is_tx_active(1));
}

TEST_CASE("Range contains", "[hal]") {
    Range r{100.0, 200.0};
    CHECK(r.contains(100.0));
    CHECK(r.contains(150.0));
    CHECK(r.contains(200.0));
    CHECK_FALSE(r.contains(99.9));
    CHECK_FALSE(r.contains(200.1));
}
