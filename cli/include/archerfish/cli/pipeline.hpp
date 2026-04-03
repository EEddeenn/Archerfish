#pragma once

#include <expected>
#include <string>

#include "archerfish/common/error.hpp"
#include "archerfish/scenario/scenario.hpp"
#include "archerfish/scenario/plan.hpp"
#include "archerfish/scenario/validator.hpp"

namespace archerfish::cli {

struct PipelineResult {
    scenario::Scenario scenario;
    scenario::ValidationResult validation;
    scenario::Plan plan;
};

[[nodiscard]] std::expected<PipelineResult, common::ErrorList> run_pipeline(const std::string& file_path);

} // namespace archerfish::cli
