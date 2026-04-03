#pragma once

#include <expected>

#include "archerfish/common/error.hpp"
#include "archerfish/scenario/plan.hpp"

namespace archerfish::scenario {

[[nodiscard]] nlohmann::json plan_to_json(const Plan& plan);

[[nodiscard]] std::expected<Plan, common::ErrorList> plan_from_json(const nlohmann::json& j);

} // namespace archerfish::scenario
