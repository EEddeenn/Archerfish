#pragma once

namespace archerfish::hal {

/// Metadata attached to a batch of TX samples.
struct TxMetadata {
    double time_spec_sec{0.0};  ///< When to send (0 = immediate)
    bool has_time_spec{false};
    bool start_of_burst{false};
    bool end_of_burst{false};
};

} // namespace archerfish::hal
