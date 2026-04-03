#pragma once

#ifdef ARCHERFISH_HAS_UHD

#include "archerfish/hal/hal_device.hpp"

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <uhd/device.hpp>
#include <uhd/usrp/multi_usrp.hpp>

namespace archerfish::hal {

class UhdDevice : public IHalDevice {
public:
    explicit UhdDevice(const uhd::device_addr_t& dev_addr);
    ~UhdDevice() override;

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

    static std::vector<uhd::device_addr_t> enumerate_uhd_devices();

private:
    void stop_all_tx();

    std::string id_;
    uhd::device_addr_t dev_addr_;
    uhd::usrp::multi_usrp::sptr usrp_;
    std::unordered_map<uint32_t, uhd::tx_streamer::sptr> tx_streamers_;
    std::unordered_map<uint32_t, bool> tx_active_;
    mutable std::mutex mutex_;
};

} // namespace archerfish::hal

#endif // ARCHERFISH_HAS_UHD
