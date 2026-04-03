#include "archerfish/cli/cmd_devices.hpp"
#include "archerfish/hal/hal_factory.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>

namespace archerfish::cli {

int cmd_devices_list(const CliOptions& opts) {
    auto devices = hal::discover_devices();

    if (opts.json_output) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& dev : devices) {
            arr.push_back({
                {"id", dev.id},
                {"type", dev.type},
                {"product", dev.product},
                {"serial", dev.serial},
                {"num_channels", dev.caps.num_channels},
                {"freq_range", {{"min", dev.caps.freq_range.min_val}, {"max", dev.caps.freq_range.max_val}}},
                {"rate_range", {{"min", dev.caps.rate_range.min_val}, {"max", dev.caps.rate_range.max_val}}},
                {"gain_range", {{"min", dev.caps.gain_range.min_val}, {"max", dev.caps.gain_range.max_val}}},
            });
        }
        fmt::print("{}\n", arr.dump(2));
    } else {
        if (devices.empty()) {
            fmt::print("No devices found.\n");
            return 0;
        }

        fmt::print("{:<16} {:<10} {:<18} {:<18} {:<18}\n", "ID", "TYPE", "PRODUCT", "FREQ RANGE (Hz)", "RATE RANGE (Sps)");
        fmt::print("{:-<16} {:-<10} {:-<18} {:-<18} {:-<18}\n", "", "", "", "", "");
        for (const auto& dev : devices) {
            fmt::print("{:<16} {:<10} {:<18} {:.2e}-{:.2e}   {:.2e}-{:.2e}\n",
                       dev.id,
                       dev.type,
                       dev.product,
                       dev.caps.freq_range.min_val, dev.caps.freq_range.max_val,
                       dev.caps.rate_range.min_val, dev.caps.rate_range.max_val);
        }
    }

    return 0;
}

int cmd_devices_info(const CliOptions& opts, const std::string& device_id) {
    auto devices = hal::discover_devices();

    auto it = std::find_if(devices.begin(), devices.end(),
        [&](const hal::DiscoveredDevice& d) { return d.id == device_id; });

    if (it == devices.end()) {
        fmt::print(stderr, "Error: device '{}' not found.\n", device_id);
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    const auto& dev = *it;
    auto device = hal::open_device(device_id);
    auto caps = device->get_capabilities();

    if (opts.json_output) {
        nlohmann::json info = {
            {"id", dev.id},
            {"type", dev.type},
            {"product", dev.product},
            {"serial", dev.serial},
            {"num_channels", caps.num_channels},
            {"freq_range", {{"min", caps.freq_range.min_val}, {"max", caps.freq_range.max_val}}},
            {"rate_range", {{"min", caps.rate_range.min_val}, {"max", caps.rate_range.max_val}}},
            {"gain_range", {{"min", caps.gain_range.min_val}, {"max", caps.gain_range.max_val}}},
            {"bandwidth_range", {{"min", caps.bandwidth_range.min_val}, {"max", caps.bandwidth_range.max_val}}},
            {"clock_sources", caps.supported_clock_sources},
            {"time_sources", caps.supported_time_sources},
            {"supports_replay", caps.supports_replay},
        };
        fmt::print("{}\n", info.dump(2));
    } else {
        fmt::print("Device: {}\n", dev.id);
        fmt::print("  Type:          {}\n", dev.type);
        fmt::print("  Product:       {}\n", dev.product);
        fmt::print("  Serial:        {}\n", dev.serial);
        fmt::print("  Channels:      {}\n", caps.num_channels);
        fmt::print("  Freq range:    {:.2e} — {:.2e} Hz\n", caps.freq_range.min_val, caps.freq_range.max_val);
        fmt::print("  Rate range:    {:.2e} — {:.2e} Sps\n", caps.rate_range.min_val, caps.rate_range.max_val);
        fmt::print("  Gain range:    {:.1f} — {:.1f} dB\n", caps.gain_range.min_val, caps.gain_range.max_val);
        fmt::print("  BW range:      {:.2e} — {:.2e} Hz\n", caps.bandwidth_range.min_val, caps.bandwidth_range.max_val);
        fmt::print("  Clock sources: ");
        for (size_t i = 0; i < caps.supported_clock_sources.size(); ++i) {
            if (i > 0) fmt::print(", ");
            fmt::print("{}", caps.supported_clock_sources[i]);
        }
        fmt::print("\n  Time sources:  ");
        for (size_t i = 0; i < caps.supported_time_sources.size(); ++i) {
            if (i > 0) fmt::print(", ");
            fmt::print("{}", caps.supported_time_sources[i]);
        }
        fmt::print("\n  Replay:        {}\n", caps.supports_replay ? "yes" : "no");
    }

    return 0;
}

} // namespace archerfish::cli
