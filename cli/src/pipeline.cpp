#include "archerfish/cli/pipeline.hpp"

#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"

namespace archerfish::cli {

std::expected<PipelineResult, common::ErrorList> run_pipeline(const std::string& file_path) {
    auto parse_result = scenario::parse_scenario(file_path);
    if (!parse_result.has_value()) {
        return std::unexpected(std::move(parse_result).error());
    }

    auto& scenario = parse_result.value();
    auto ref_errors = scenario::resolve_waveform_refs(scenario);
    if (!ref_errors.empty()) {
        return std::unexpected(std::move(ref_errors));
    }

    auto val_result = scenario::validate(scenario);
    if (!val_result.ok()) {
        return std::unexpected(std::move(val_result.errors));
    }

    auto plan_result = scenario::plan(scenario);
    if (!plan_result.has_value()) {
        return std::unexpected(std::move(plan_result).error());
    }

    return PipelineResult{
        .scenario = std::move(scenario),
        .validation = std::move(val_result),
        .plan = std::move(plan_result.value()),
    };
}

} // namespace archerfish::cli
