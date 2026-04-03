#pragma once

#include <string>
#include <fmt/format.h>

namespace archerfish::cli {

[[nodiscard]] inline std::string format_freq(double hz) {
    if (hz >= 1e9) return fmt::format("{:.1f} GHz", hz / 1e9);
    if (hz >= 1e6) return fmt::format("{:.1f} MHz", hz / 1e6);
    if (hz >= 1e3) return fmt::format("{:.1f} kHz", hz / 1e3);
    return fmt::format("{:.0f} Hz", hz);
}

[[nodiscard]] inline std::string format_rate(double sps) {
    if (sps >= 1e6) return fmt::format("{:.0f} MSps", sps / 1e6);
    if (sps >= 1e3) return fmt::format("{:.0f} kSps", sps / 1e3);
    return fmt::format("{:.0f} Sps", sps);
}

[[nodiscard]] inline std::string format_time(double sec) {
    if (sec < 0.001) return fmt::format("{:.0f}us", sec * 1e6);
    if (sec < 1.0) return fmt::format("{:.1f}ms", sec * 1e3);
    return fmt::format("{:.3f}s", sec);
}

} // namespace archerfish::cli
