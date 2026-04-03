#pragma once

#include <archerfish/hal/device_capabilities.hpp>
#include <archerfish/hal/hal_device.hpp>

#include <memory>
#include <string>
#include <vector>

namespace archerfish::hal {

struct DiscoveredDevice {
    std::string id;
    std::string type;
    std::string product;
    std::string serial;
    DeviceCapabilities caps;
};

std::vector<DiscoveredDevice> discover_devices();
std::unique_ptr<IHalDevice> open_device(const std::string& device_id);

} // namespace archerfish::hal
