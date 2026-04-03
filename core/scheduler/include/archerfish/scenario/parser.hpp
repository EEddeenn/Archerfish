#pragma once

#include <filesystem>
#include <expected>
#include <string>

#include "archerfish/common/error.hpp"
#include "archerfish/scenario/scenario.hpp"

namespace archerfish::scenario {

[[nodiscard]] std::expected<Scenario, common::ErrorList> parse_scenario(const std::filesystem::path& json_path);

[[nodiscard]] std::expected<Scenario, common::ErrorList> parse_scenario_json(const std::string& json_str);

[[nodiscard]] common::ErrorList resolve_waveform_refs(Scenario& scenario);

} // namespace archerfish::scenario
