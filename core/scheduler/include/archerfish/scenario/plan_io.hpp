#pragma once

#include <expected>

#include "archerfish/common/error.hpp"
#include "archerfish/scenario/scenario.hpp"
#include "archerfish/scenario/plan.hpp"

namespace archerfish::scenario {

// --- Scenario serialization ---
[[nodiscard]] nlohmann::json metadata_to_json(const Metadata& m);
[[nodiscard]] std::expected<Metadata, common::ErrorList> metadata_from_json(const nlohmann::json& j);

[[nodiscard]] nlohmann::json rf_settings_to_json(const RfSettings& rf);
[[nodiscard]] std::expected<RfSettings, common::ErrorList> rf_settings_from_json(const nlohmann::json& j);

[[nodiscard]] nlohmann::json waveform_def_to_json(const WaveformDef& wf);
[[nodiscard]] std::expected<WaveformDef, common::ErrorList> waveform_def_from_json(const nlohmann::json& j);

[[nodiscard]] nlohmann::json device_def_to_json(const DeviceDef& d);
[[nodiscard]] std::expected<DeviceDef, common::ErrorList> device_def_from_json(const nlohmann::json& j);

[[nodiscard]] nlohmann::json emitter_def_to_json(const EmitterDef& e);
[[nodiscard]] std::expected<EmitterDef, common::ErrorList> emitter_def_from_json(const nlohmann::json& j);

[[nodiscard]] nlohmann::json reporting_config_to_json(const ReportingConfig& r);
[[nodiscard]] ReportingConfig reporting_config_from_json(const nlohmann::json& j);

[[nodiscard]] nlohmann::json channel_def_to_json(const ChannelDef& c);
[[nodiscard]] std::expected<ChannelDef, common::ErrorList> channel_def_from_json(const nlohmann::json& j);

[[nodiscard]] nlohmann::json sync_group_to_json(const SyncGroup& sg);
[[nodiscard]] std::expected<SyncGroup, common::ErrorList> sync_group_from_json(const nlohmann::json& j);

[[nodiscard]] nlohmann::json scenario_to_json(const Scenario& s);
[[nodiscard]] std::expected<Scenario, common::ErrorList> scenario_from_json(const nlohmann::json& j);

// --- Error serialization ---
[[nodiscard]] nlohmann::json error_to_json(const common::Error& err);
[[nodiscard]] common::Error error_from_json(const nlohmann::json& j);

// --- Plan serialization ---
[[nodiscard]] nlohmann::json plan_to_json(const Plan& plan);
[[nodiscard]] std::expected<Plan, common::ErrorList> plan_from_json(const nlohmann::json& j);

} // namespace archerfish::scenario
