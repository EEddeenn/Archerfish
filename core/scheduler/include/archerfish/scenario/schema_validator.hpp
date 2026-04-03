#pragma once

#include <expected>
#include <nlohmann/json.hpp>

#include "archerfish/common/error.hpp"

namespace archerfish::scenario {

/// Validate a JSON instance against the built-in scenario schema.
/// Returns void on success, or an ErrorList of all collected validation errors.
[[nodiscard]] std::expected<void, common::ErrorList> validate_schema(const nlohmann::json& instance);

/// Load the built-in scenario schema as a JSON object.
[[nodiscard]] nlohmann::json get_scenario_schema();

} // namespace archerfish::scenario
