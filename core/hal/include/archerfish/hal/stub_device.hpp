#pragma once

#include "archerfish/hal/hal_device.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace archerfish::hal {

struct DeviceCallRecord {
    std::string method;
    uint32_t channel{0};
    double value{0.0};
    std::string str_value;
    size_t sample_count{0};
    TxMetadata metadata;
};

class StubDevice : public IHalDevice {
public:
    explicit StubDevice(std::string id = "stub0",
                        DeviceCapabilities caps = DeviceCapabilities{});

    [[nodiscard]] std::string device_id() const override;
    [[nodiscard]] DeviceCapabilities get_capabilities() const override;

    void set_center_freq(uint32_t channel, double hz) override;
    void set_sample_rate(uint32_t channel, double sps) override;
    void set_bandwidth(uint32_t channel, double hz) override;
    void set_gain(uint32_t channel, double db) override;
    void set_antenna(uint32_t channel, std::string_view port) override;

    void set_clock_source(std::string_view source) override;
    void set_time_source(std::string_view source) override;
    void sync_time_now() override;

    void start_tx(uint32_t channel) override;
    void stop_tx(uint32_t channel) override;

    void send_samples(uint32_t channel,
                      const std::complex<float>* data,
                      size_t count,
                      const TxMetadata& meta) override;

    [[nodiscard]] bool is_tx_active(uint32_t channel) const override;

    [[nodiscard]] const std::vector<DeviceCallRecord>& call_history() const;
    [[nodiscard]] size_t total_samples_sent(uint32_t channel) const;
    void reset();

private:
    void record(std::string method, uint32_t ch, double val = 0.0);
    void record_str(std::string method, uint32_t ch, std::string_view sv);
    void record_samples(uint32_t ch, size_t count, const TxMetadata& meta);

    std::string id_;
    DeviceCapabilities caps_;
    std::vector<DeviceCallRecord> calls_;
    std::unordered_map<uint32_t, bool> tx_active_;
    std::unordered_map<uint32_t, double> freq_;
    std::unordered_map<uint32_t, double> rate_;
    std::unordered_map<uint32_t, double> gain_;
    std::unordered_map<uint32_t, double> bw_;
    std::unordered_map<uint32_t, size_t> samples_sent_;
};

} // namespace archerfish::hal
