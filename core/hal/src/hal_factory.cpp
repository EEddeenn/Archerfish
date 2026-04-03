#include <archerfish/hal/hal_factory.hpp>
#include <archerfish/hal/stub_device.hpp>

#ifdef ARCHERFISH_HAS_UHD
#include <archerfish/hal/uhd_device.hpp>
#endif

#include <spdlog/spdlog.h>

#include <mutex>
#include <stdexcept>

namespace archerfish::hal {

namespace {
struct UhdDeviceInfo {
    uhd::device_addr_t addr;
    std::string id;
};

std::mutex& cache_mutex() {
    static std::mutex m;
    return m;
}

std::vector<UhdDeviceInfo>& cached_uhd_devices() {
    static std::vector<UhdDeviceInfo> devices;
    return devices;
}

void discover_uhd_into(std::vector<DiscoveredDevice>& out) {
#ifdef ARCHERFISH_HAS_UHD
    auto found = UhdDevice::enumerate_uhd_devices();
    for (auto& addr : found) {
        std::string serial = addr.has_key("serial") ? addr["serial"] : "unknown";
        std::string product = addr.has_key("product") ? addr["product"] : "usrp";
        std::string id = fmt::format("usrp-{}-{}", product, serial);

        out.push_back({id, "usrp", product, serial, DeviceCapabilities{}});
        cached_uhd_devices().push_back({addr, id});
    }
#else
    spdlog::debug("UHD support not compiled in");
#endif
}
} // namespace

std::vector<DiscoveredDevice> discover_devices() {
    std::lock_guard<std::mutex> lock(cache_mutex());
    std::vector<DiscoveredDevice> devices;
    cached_uhd_devices().clear();

    discover_uhd_into(devices);

    devices.push_back({
        "stub0",
        "stub",
        "stub",
        "",
        DeviceCapabilities{}
    });

    return devices;
}

std::unique_ptr<IHalDevice> open_device(const std::string& device_id) {
    if (device_id == "stub0") {
        return std::make_unique<StubDevice>("stub0");
    }

#ifdef ARCHERFISH_HAS_UHD
    {
        std::lock_guard<std::mutex> lock(cache_mutex());
        for (const auto& info : cached_uhd_devices()) {
            if (info.id == device_id) {
                return std::make_unique<UhdDevice>(info.addr);
            }
        }
    }
    for (auto& addr : UhdDevice::enumerate_uhd_devices()) {
        std::string serial = addr.has_key("serial") ? addr["serial"] : "unknown";
        std::string product = addr.has_key("product") ? addr["product"] : "usrp";
        std::string id = fmt::format("usrp-{}-{}", product, serial);
        if (id == device_id) {
            return std::make_unique<UhdDevice>(addr);
        }
    }
#endif

    throw std::runtime_error(fmt::format("Device '{}' not found", device_id));
}

} // namespace archerfish::hal
