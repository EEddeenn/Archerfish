#pragma once

#include <archerfish/hal/device_capabilities.hpp>
#include <archerfish/hal/tx_metadata.hpp>

#include <complex>
#include <cstdint>
#include <string>
#include <string_view>

namespace archerfish::hal {

/// Abstract Hardware Abstraction Layer device interface.
///
/// This is the sole boundary between Archerfish core and UHD.
/// All RF control, clock/time synchronisation, and sample streaming
/// go through this interface.
class IHalDevice {
public:
    virtual ~IHalDevice() = default;

    // --- Device info -------------------------------------------------------

    /// Opaque identifier for this device (e.g. "usrp0", "stub0").
    [[nodiscard]] virtual std::string device_id() const = 0;

    /// Static capabilities of the underlying hardware.
    [[nodiscard]] virtual DeviceCapabilities get_capabilities() const = 0;

    // --- RF control --------------------------------------------------------

    virtual void set_center_freq(uint32_t channel, double hz) = 0;
    virtual void set_sample_rate(uint32_t channel, double sps) = 0;
    virtual void set_bandwidth(uint32_t channel, double hz) = 0;
    virtual void set_gain(uint32_t channel, double db) = 0;
    virtual void set_antenna(uint32_t channel, std::string_view port) = 0;

    // --- Clock / time control ----------------------------------------------

    virtual void set_clock_source(std::string_view source) = 0;
    virtual void set_time_source(std::string_view source) = 0;
    virtual void sync_time_now() = 0;

    // --- TX control --------------------------------------------------------

    virtual void start_tx(uint32_t channel) = 0;
    virtual void stop_tx(uint32_t channel) = 0;

    // --- Streaming ---------------------------------------------------------

    virtual void send_samples(uint32_t channel,
                              const std::complex<float>* data,
                              size_t count,
                              const TxMetadata& meta) = 0;

    // --- Status ------------------------------------------------------------

    [[nodiscard]] virtual bool is_tx_active(uint32_t channel) const = 0;
};

} // namespace archerfish::hal
