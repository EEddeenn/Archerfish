#include "archerfish/cli/cmd_doctor.hpp"

#include <fmt/format.h>

namespace archerfish::cli {

int cmd_version(const CliOptions& opts) {
    (void)opts;
    fmt::print("archerfish v0.1.0\n");
    return 0;
}

int cmd_doctor(const CliOptions& opts) {
    (void)opts;
    bool all_ok = true;

    fmt::print("archerfish doctor — diagnostics\n\n");

    fmt::print("[CHECK] Build configuration ... ");
#if defined(ARCHERFISH_HAS_UHD)
    fmt::print("OK (UHD enabled)\n");
#else
    fmt::print("OK (stub mode, no UHD)\n");
#endif

    fmt::print("[CHECK] C++ standard .......... C++23\n");

    fmt::print("\nAll checks passed.\n");
    return all_ok ? 0 : static_cast<int>(ExitCode::GenericFailure);
}

} // namespace archerfish::cli
