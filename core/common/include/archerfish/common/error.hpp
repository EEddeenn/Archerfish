#pragma once

#include <string>
#include <vector>

namespace archerfish::common {

/// Broad category for errors that can occur during scenario execution.
enum class ErrorCategory {
    Config,
    Validation,
    Planning,
    Preparation,
    Execution,
    QualityWarning
};

/// A single error or warning item.
struct Error {
    ErrorCategory category;
    std::string code;      ///< machine-readable code e.g. "E001_INVALID_AMPLITUDE"
    std::string message;   ///< human-readable description
};

/// Ordered list of errors / warnings.
using ErrorList = std::vector<Error>;

/// Convert an ErrorCategory to its string representation.
[[nodiscard]] std::string category_to_string(ErrorCategory cat);

/// Convert a string back to an ErrorCategory. Returns Config for unknown strings.
[[nodiscard]] ErrorCategory category_from_string(const std::string& s);

/// Format an Error as "category: code — message".
[[nodiscard]] std::string error_to_string(const Error& err);

} // namespace archerfish::common
