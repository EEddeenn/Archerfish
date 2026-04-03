#pragma once

#include <expected>

#include "archerfish/common/error.hpp"
#include "archerfish/scenario/plan.hpp"

namespace archerfish::scenario {

[[nodiscard]] std::expected<Plan, common::ErrorList> plan(const Scenario& scenario);

} // namespace archerfish::scenario
