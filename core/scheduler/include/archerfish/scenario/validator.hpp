#pragma once

#include "archerfish/scenario/scenario.hpp"
#include "archerfish/common/error.hpp"

namespace archerfish::scenario {

/// Validation result: errors (blocking) and warnings (non-blocking).
struct ValidationResult {
    common::ErrorList errors;
    common::ErrorList warnings;
    [[nodiscard]] bool ok() const { return errors.empty(); }
};

/// Validate a parsed scenario for semantic correctness.
/// This checks business rules beyond JSON schema validation.
[[nodiscard]] ValidationResult validate(const Scenario& scenario);

} // namespace archerfish::scenario
