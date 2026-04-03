#ifdef ARCHERFISH_HAS_UHD

#include <archerfish/hal/uhd_device.hpp>

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace archerfish::hal {

UhdDevice::UhdDevice(const uhd::device_addr_t& dev_addr)
    : dev_addr_(dev_addr) {
    std::string serial = dev_addr.has_key("serial") ? dev_addr["serial"] : "unknown";
    std::string product = dev_addr.has_key("product") ? dev_addr["product"] : "usrp";
    id_ = fmt::format("usrp-{}-{}", product, serial);

    spdlog::info("Opening USRP device: {}", id_);
    usrp_ = uhd::usrp::multi_usrp::make(dev_addr);
    spdlog::info("USRP device opened: {} ({})", id_, usrp_->get_mboard_name());
}

std::string UhdDevice::device_id() const {
    return id_;
}

DeviceCapabilities UhdDevice::get_capabilities() const {
    DeviceCapabilities caps;
    caps.num_channels = usrp_->get_num_mboards();

    try {
        auto tx_freq_range = usrp_->get_tx_freq_range(0);
        caps.freq_range.min_val = tx_freq_range.start();
        caps.freq_range.max_val = tx_freq_range.stop();
    } catch (const uhd::exception::runtime_error&) {
        spdlog::warn("Could not query TX freq range, using defaults");
    }

    try {
        auto clock_rates = usrp_->get_master_clock_rate_range();
        caps.rate_range.min_val = clock_rates.start();
        caps.rate_range.max_val = clock_rates.stop();
    } catch (const uhd::exception::runtime_error&) {
        spdlog::warn("Could not query master clock rate range, using defaults");
    }

    try {
        auto tx_gain_range = usrp_->get_tx_gain_range(0);
        caps.gain_range.min_val = tx_gain_range.start();
        caps.gain_range.max_val = tx_gain_range.stop();
    } catch (const uhd::exception::runtime_error&) {
        spdlog::warn("Could not query TX gain range, using defaults");
    }

    try {
        auto tx_bw_range = usrp_->get_tx_bandwidth_range(0);
        caps.bandwidth_range.min_val = tx_bw_range.start();
        caps.bandwidth_range.max_val = tx_bw_range.stop();
    } catch (const uhd::exception::runtime_error&) {
        spdlog::warn("Could not query TX bandwidth range, using defaults");
    }

    try {
        caps.supported_clock_sources = usrp_->get_clock_sources(0);
    } catch (const uhd::exception::runtime_error&) {
        spdlog::warn("Could not query clock sources");
    }

    try {
        caps.supported_time_sources = usrp_->get_time_sources(0);
    } catch (const uhd::exception::runtime_error&) {
        spdlog::warn("Could not query time sources");
    }

    return caps;
}

void UhdDevice::set_center_freq(uint32_t channel, double hz) {
    usrp_->set_tx_freq(hz, channel);
    spdlog::debug("CH{} set_center_freq: {:.2e} Hz", channel, hz);
}

void UhdDevice::set_sample_rate(uint32_t channel, double sps) {
    usrp_->set_tx_rate(sps, channel);
    spdlog::debug("CH{} set_sample_rate: {:.2e} Sps", channel, sps);
}

void UhdDevice::set_bandwidth(uint32_t channel, double hz) {
    usrp_->set_tx_bandwidth(hz, channel);
    spdlog::debug("CH{} set_bandwidth: {:.2e} Hz", channel, hz);
}

void UhdDevice::set_gain(uint32_t channel, double db) {
    usrp_->set_tx_gain(db, channel);
    spdlog::debug("CH{} set_gain: {:.1f} dB", channel, db);
}

void UhdDevice::set_antenna(uint32_t channel, std::string_view port) {
    usrp_->set_tx_antenna(std::string(port), channel);
    spdlog::debug("CH{} set_antenna: {}", channel, port);
}

void UhdDevice::set_clock_source(std::string_view source) {
    usrp_->set_clock_source(std::string(source));
    spdlog::debug("set_clock_source: {}", source);
}

void UhdDevice::set_time_source(std::string_view source) {
    usrp_->set_time_source(std::string(source));
    spdlog::debug("set_time_source: {}", source);
}

void UhdDevice::sync_time_now() {
    usrp_->set_time_now(uhd::time_spec_t(0.0));
    spdlog::debug("sync_time_now");
}

void UhdDevice::start_tx(uint32_t channel) {
    tx_streamer_ = usrp_->get_tx_stream(uhd::stream_args_t("fc32"));
    tx_active_[channel] = true;
    spdlog::info("CH{} TX started (streamer created, spb={})", channel, tx_streamer_->get_max_num_samps());
}

void UhdDevice::stop_tx(uint32_t channel) {
    if (tx_streamer_) {
        uhd::tx_metadata_t md;
        md.end_of_burst = true;
        std::vector<std::complex<float>> empty;
        tx_streamer_->send(empty.data(), 0, md);
    }
    tx_streamer_ = nullptr;
    tx_active_[channel] = false;
    spdlog::info("CH{} TX stopped", channel);
}

void UhdDevice::send_samples(uint32_t channel,
                               const std::complex<float>* data,
                               size_t count,
                               const TxMetadata& meta) {
    if (!tx_streamer_) {
        spdlog::error("send_samples called without active TX streamer");
        return;
    }
    uhd::tx_metadata_t md;
    md.has_time_spec = meta.has_time_spec;
    md.time_spec = uhd::time_spec_t(meta.time_spec_sec);
    md.start_of_burst = meta.start_of_burst;
    md.end_of_burst = meta.end_of_burst;
    tx_streamer_->send(data, count, md);
}

bool UhdDevice::is_tx_active(uint32_t channel) const {
    auto it = tx_active_.find(channel);
    return it != tx_active_.end() && it->second;
}

std::vector<uhd::device_addr_t> UhdDevice::enumerate_uhd_devices() {
    try {
        return uhd::device::find(uhd::device_addr_t());
    } catch (const std::exception& e) {
        spdlog::warn("UHD device enumeration failed: {}", e.what());
        return {};
    }
}

} // namespace archerfish::hal

#endif // ARCHERFISH_HAS_UHD
