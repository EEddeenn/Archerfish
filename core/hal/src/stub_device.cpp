#include <archerfish/hal/stub_device.hpp>

namespace archerfish::hal {

StubDevice::StubDevice(std::string id, DeviceCapabilities caps)
    : id_(std::move(id)), caps_(std::move(caps)) {}

std::string StubDevice::device_id() const {
    return id_;
}

DeviceCapabilities StubDevice::get_capabilities() const {
    return caps_;
}

void StubDevice::set_center_freq(uint32_t channel, double hz) {
    freq_[channel] = hz;
    record("set_center_freq", channel, hz);
}

void StubDevice::set_sample_rate(uint32_t channel, double sps) {
    rate_[channel] = sps;
    record("set_sample_rate", channel, sps);
}

void StubDevice::set_bandwidth(uint32_t channel, double hz) {
    bw_[channel] = hz;
    record("set_bandwidth", channel, hz);
}

void StubDevice::set_gain(uint32_t channel, double db) {
    gain_[channel] = db;
    record("set_gain", channel, db);
}

void StubDevice::set_antenna(uint32_t channel, std::string_view port) {
    record_str("set_antenna", channel, port);
}

void StubDevice::set_clock_source(std::string_view source) {
    record_str("set_clock_source", 0, source);
}

void StubDevice::set_time_source(std::string_view source) {
    record_str("set_time_source", 0, source);
}

void StubDevice::sync_time_now() {
    record("sync_time_now", 0);
}

void StubDevice::start_tx(uint32_t channel) {
    tx_active_[channel] = true;
    record("start_tx", channel);
}

void StubDevice::stop_tx(uint32_t channel) {
    tx_active_[channel] = false;
    record("stop_tx", channel);
}

void StubDevice::send_samples(uint32_t channel,
                              const std::complex<float>* /*data*/,
                              size_t count,
                              const TxMetadata& meta) {
    if (!is_tx_active(channel)) {
        return;
    }
    samples_sent_[channel] += count;
    record_samples(channel, count, meta);
}

bool StubDevice::is_tx_active(uint32_t channel) const {
    auto it = tx_active_.find(channel);
    return it != tx_active_.end() && it->second;
}

const std::vector<DeviceCallRecord>& StubDevice::call_history() const {
    return calls_;
}

size_t StubDevice::total_samples_sent(uint32_t channel) const {
    auto it = samples_sent_.find(channel);
    return it != samples_sent_.end() ? it->second : 0;
}

void StubDevice::reset() {
    calls_.clear();
    tx_active_.clear();
    freq_.clear();
    rate_.clear();
    gain_.clear();
    bw_.clear();
    samples_sent_.clear();
}

void StubDevice::record(std::string method, uint32_t ch, double val) {
    calls_.push_back({.method = std::move(method), .channel = ch, .value = val});
}

void StubDevice::record_str(std::string method, uint32_t ch, std::string_view sv) {
    calls_.push_back({.method = std::move(method), .channel = ch, .str_value = std::string(sv)});
}

void StubDevice::record_samples(uint32_t ch, size_t count, const TxMetadata& meta) {
    calls_.push_back({.method = "send_samples", .channel = ch, .sample_count = count, .metadata = meta});
}

} // namespace archerfish::hal
