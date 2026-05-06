#include "archerfish/scenario/parser.hpp"
#include "archerfish/dsp/waveform_type.hpp"
using archerfish::dsp::WaveformType;

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "archerfish/scenario/planner.hpp"
#include "archerfish/scenario/plan_io.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::ContainsSubstring;

namespace {

Scenario make_simple_scenario() {
    Scenario s;
    s.metadata.name = "test_io";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0, 8e6, "TX/RX"}});

    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 1.0;
    em.duration_sec = 5.0;
    em.waveform = WaveformDef{"cw_wf", WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em);

    return s;
}

Plan make_simple_plan() {
    auto result = plan(make_simple_scenario());
    return std::move(result.value());
}

} // namespace

TEST_CASE("plan_to_json contains expected top-level keys", "[plan_io]") {
    auto p = make_simple_plan();
    auto j = plan_to_json(p);

    REQUIRE(j.contains("normalized_scenario"));
    REQUIRE(j.contains("channels"));
    REQUIRE(j.contains("timeline"));
    REQUIRE(j.contains("render_instructions"));
    REQUIRE(j.contains("warnings"));
    REQUIRE(j.contains("estimated_duration_sec"));
}

TEST_CASE("Round-trip: Plan -> JSON -> Plan preserves all fields", "[plan_io]") {
    auto original = make_simple_plan();
    auto json = plan_to_json(original);

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());

    const auto& restored = restored_result.value();

    REQUIRE(restored.channels.size() == original.channels.size());
    REQUIRE(restored.channels[0].device_id == original.channels[0].device_id);
    REQUIRE(restored.channels[0].channel_index == original.channels[0].channel_index);
    REQUIRE_THAT(restored.channels[0].rf.freq_hz, WithinAbs(original.channels[0].rf.freq_hz, 1e-12));
    REQUIRE_THAT(restored.channels[0].rf.rate_sps, WithinAbs(original.channels[0].rf.rate_sps, 1e-12));
    REQUIRE_THAT(restored.channels[0].rf.gain_db, WithinAbs(original.channels[0].rf.gain_db, 1e-12));

    REQUIRE(restored.timeline.size() == original.timeline.size());
    for (size_t i = 0; i < original.timeline.size(); ++i) {
        REQUIRE(restored.timeline[i].type == original.timeline[i].type);
        REQUIRE_THAT(restored.timeline[i].time_sec, WithinAbs(original.timeline[i].time_sec, 1e-12));
        REQUIRE(restored.timeline[i].target_id == original.timeline[i].target_id);
    }

    REQUIRE(restored.render_instructions.size() == original.render_instructions.size());
    for (size_t i = 0; i < original.render_instructions.size(); ++i) {
        REQUIRE(restored.render_instructions[i].emitter_id == original.render_instructions[i].emitter_id);
        REQUIRE(restored.render_instructions[i].waveform.type == original.render_instructions[i].waveform.type);
        REQUIRE_THAT(restored.render_instructions[i].start_sec, WithinAbs(original.render_instructions[i].start_sec, 1e-12));
        REQUIRE_THAT(restored.render_instructions[i].duration_sec, WithinAbs(original.render_instructions[i].duration_sec, 1e-12));
        REQUIRE_THAT(restored.render_instructions[i].sample_rate, WithinAbs(original.render_instructions[i].sample_rate, 1e-12));
    }

    REQUIRE_THAT(restored.estimated_duration_sec, WithinAbs(original.estimated_duration_sec, 1e-12));

    REQUIRE(restored.warnings.size() == original.warnings.size());
    for (size_t i = 0; i < original.warnings.size(); ++i) {
        REQUIRE(restored.warnings[i].code == original.warnings[i].code);
        REQUIRE(restored.warnings[i].message == original.warnings[i].message);
    }
}

TEST_CASE("Timeline events serialize and deserialize correctly", "[plan_io]") {
    auto original = make_simple_plan();
    auto json = plan_to_json(original);

    REQUIRE(json["timeline"].is_array());
    REQUIRE(json["timeline"].size() == 2);

    REQUIRE(json["timeline"][0]["type"].get<std::string>() == "EmitterStart");
    REQUIRE_THAT(json["timeline"][0]["time_sec"].get<double>(), WithinAbs(1.0, 1e-12));
    REQUIRE(json["timeline"][0]["target_id"].get<std::string>() == "cw1");

    REQUIRE(json["timeline"][1]["type"].get<std::string>() == "EmitterStop");
    REQUIRE_THAT(json["timeline"][1]["time_sec"].get<double>(), WithinAbs(6.0, 1e-12));

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().timeline[0].type == TimelineEventType::EmitterStart);
    REQUIRE(restored_result.value().timeline[1].type == TimelineEventType::EmitterStop);
}

TEST_CASE("Render instructions without resample_ratio round-trip", "[plan_io]") {
    auto original = make_simple_plan();
    REQUIRE_FALSE(original.render_instructions[0].resample_ratio.has_value());

    auto json = plan_to_json(original);
    REQUIRE_FALSE(json["render_instructions"][0].contains("resample_ratio"));

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE_FALSE(restored_result.value().render_instructions[0].resample_ratio.has_value());
}

TEST_CASE("Render instructions with resample_ratio round-trip", "[plan_io]") {
    auto p = make_simple_plan();
    p.render_instructions[0].resample_ratio = 1.5;

    auto json = plan_to_json(p);
    REQUIRE(json["render_instructions"][0].contains("resample_ratio"));
    REQUIRE_THAT(json["render_instructions"][0]["resample_ratio"].get<double>(), WithinAbs(1.5, 1e-12));

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().render_instructions[0].resample_ratio.has_value());
    REQUIRE_THAT(*restored_result.value().render_instructions[0].resample_ratio, WithinAbs(1.5, 1e-12));
}

TEST_CASE("Plan import rejects waveform params with reserved type key", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());
    json["render_instructions"][0]["waveform"]["params"]["type"] = "noise";

    auto restored_result = plan_from_json(json);
    REQUIRE_FALSE(restored_result.has_value());
    REQUIRE(restored_result.error().size() == 1);
    REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_RESERVED_WAVEFORM_PARAM");
}

TEST_CASE("Plan import rejects non-object waveform params", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());
    json["render_instructions"][0]["waveform"]["params"] = nlohmann::json::array({1, 2});

    auto restored_result = plan_from_json(json);
    REQUIRE_FALSE(restored_result.has_value());
    REQUIRE(restored_result.error().size() == 1);
    REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
}

TEST_CASE("Plan import normalizes null waveform params to an empty object", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());
    json["render_instructions"][0]["waveform"]["params"] = nullptr;

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().render_instructions[0].waveform.params.is_object());
    REQUIRE(restored_result.value().render_instructions[0].waveform.params.empty());
}

TEST_CASE("Warnings serialize and deserialize correctly", "[plan_io]") {
    auto p = make_simple_plan();
    p.warnings.push_back({ErrorCategory::QualityWarning, "W_TEST", "test warning"});

    auto json = plan_to_json(p);
    REQUIRE(json["warnings"].is_array());

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().warnings.size() == p.warnings.size());
    REQUIRE(restored_result.value().warnings.back().code == "W_TEST");
    REQUIRE(restored_result.value().warnings.back().message == "test warning");
}

TEST_CASE("Malformed diagnostic entries fail plan import", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());
    json["warnings"] = nlohmann::json::array({{{"category", 42}, {"code", "W_TEST"}, {"message", "test"}}});

    auto restored_result = plan_from_json(json);
    REQUIRE_FALSE(restored_result.has_value());
    REQUIRE(restored_result.error().size() == 1);
    CHECK(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");

    auto bad_entry = plan_to_json(make_simple_plan());
    bad_entry["warnings"] = nlohmann::json::array({"not an object"});
    auto bad_entry_result = plan_from_json(bad_entry);
    REQUIRE_FALSE(bad_entry_result.has_value());
    CHECK(bad_entry_result.error()[0].code == "E_PLAN_IO_BAD_JSON");

    auto bad_category = plan_to_json(make_simple_plan());
    bad_category["warnings"] = nlohmann::json::array({
        {{"category", "Bogus"}, {"code", "W_TEST"}, {"message", "test"}},
    });
    auto bad_category_result = plan_from_json(bad_category);
    REQUIRE_FALSE(bad_category_result.has_value());
    CHECK(bad_category_result.error()[0].code == "E_PLAN_IO_BAD_JSON");

    auto missing_message = plan_to_json(make_simple_plan());
    missing_message["warnings"] = nlohmann::json::array({
        {{"category", "QualityWarning"}, {"code", "W_TEST"}},
    });
    auto missing_message_result = plan_from_json(missing_message);
    REQUIRE_FALSE(missing_message_result.has_value());
    CHECK(missing_message_result.error()[0].code == "E_PLAN_IO_BAD_JSON");

    auto empty_code = plan_to_json(make_simple_plan());
    empty_code["warnings"] = nlohmann::json::array({
        {{"category", "QualityWarning"}, {"code", ""}, {"message", "test"}},
    });
    auto empty_code_result = plan_from_json(empty_code);
    REQUIRE_FALSE(empty_code_result.has_value());
    CHECK(empty_code_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
    REQUIRE_THAT(empty_code_result.error()[0].message, ContainsSubstring("warnings[].code"));

    auto empty_message = plan_to_json(make_simple_plan());
    empty_message["warnings"] = nlohmann::json::array({
        {{"category", "QualityWarning"}, {"code", "W_TEST"}, {"message", ""}},
    });
    auto empty_message_result = plan_from_json(empty_message);
    REQUIRE_FALSE(empty_message_result.has_value());
    CHECK(empty_message_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
    REQUIRE_THAT(empty_message_result.error()[0].message, ContainsSubstring("warnings[].message"));
}

TEST_CASE("Plan import rejects negative timing and rate fields", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    auto negative_timeline = json;
    negative_timeline["timeline"][0]["time_sec"] = -0.1;
    CHECK_FALSE(plan_from_json(negative_timeline).has_value());

    auto negative_render_start = json;
    negative_render_start["render_instructions"][0]["start_sec"] = -0.1;
    CHECK_FALSE(plan_from_json(negative_render_start).has_value());

    auto negative_render_duration = json;
    negative_render_duration["render_instructions"][0]["duration_sec"] = -1.0;
    CHECK_FALSE(plan_from_json(negative_render_duration).has_value());

    auto zero_render_duration = json;
    zero_render_duration["render_instructions"][0]["duration_sec"] = 0.0;
    CHECK_FALSE(plan_from_json(zero_render_duration).has_value());

    auto zero_sample_rate = json;
    zero_sample_rate["render_instructions"][0]["sample_rate"] = 0.0;
    CHECK_FALSE(plan_from_json(zero_sample_rate).has_value());

    auto negative_resample_ratio = json;
    negative_resample_ratio["render_instructions"][0]["resample_ratio"] = -1.0;
    CHECK_FALSE(plan_from_json(negative_resample_ratio).has_value());

    auto negative_mix_duration = json;
    negative_mix_duration["mix_groups"][0]["duration_sec"] = -1.0;
    CHECK_FALSE(plan_from_json(negative_mix_duration).has_value());

    auto zero_mix_duration = json;
    zero_mix_duration["mix_groups"][0]["duration_sec"] = 0.0;
    CHECK_FALSE(plan_from_json(zero_mix_duration).has_value());

    auto negative_resource_load = json;
    negative_resource_load["resource_estimate"]["estimated_cpu_load"] = -0.01;
    CHECK_FALSE(plan_from_json(negative_resource_load).has_value());

    auto negative_estimated_duration = json;
    negative_estimated_duration["estimated_duration_sec"] = -1.0;
    CHECK_FALSE(plan_from_json(negative_estimated_duration).has_value());
}

TEST_CASE("Plan import rejects duplicate execution identifiers", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    auto duplicate_channel = json;
    duplicate_channel["channels"].push_back(duplicate_channel["channels"][0]);
    auto duplicate_channel_result = plan_from_json(duplicate_channel);
    REQUIRE_FALSE(duplicate_channel_result.has_value());
    REQUIRE(duplicate_channel_result.error()[0].code == "E_PLAN_IO_DUPLICATE_CHANNEL_BINDING");
    REQUIRE_THAT(duplicate_channel_result.error()[0].message,
                 ContainsSubstring("channels"));

    auto duplicate_channel_index = json;
    duplicate_channel_index["channels"].push_back(duplicate_channel_index["channels"][0]);
    duplicate_channel_index["channels"][1]["device_id"] = "usrp1";
    auto duplicate_channel_index_result = plan_from_json(duplicate_channel_index);
    REQUIRE_FALSE(duplicate_channel_index_result.has_value());
    REQUIRE(duplicate_channel_index_result.error()[0].code == "E_PLAN_IO_DUPLICATE_CHANNEL_INDEX");
    REQUIRE_THAT(duplicate_channel_index_result.error()[0].message,
                 ContainsSubstring("channel_index"));

    auto duplicate_channel_plan = json;
    duplicate_channel_plan["channel_plans"].push_back(duplicate_channel_plan["channel_plans"][0]);
    auto duplicate_channel_plan_result = plan_from_json(duplicate_channel_plan);
    REQUIRE_FALSE(duplicate_channel_plan_result.has_value());
    REQUIRE(duplicate_channel_plan_result.error()[0].code == "E_PLAN_IO_DUPLICATE_CHANNEL_PLAN");
    REQUIRE_THAT(duplicate_channel_plan_result.error()[0].message,
                 ContainsSubstring("channel_plans.channel_index"));

    auto duplicate_render = json;
    duplicate_render["render_instructions"].push_back(duplicate_render["render_instructions"][0]);
    auto duplicate_render_result = plan_from_json(duplicate_render);
    REQUIRE_FALSE(duplicate_render_result.has_value());
    REQUIRE(duplicate_render_result.error()[0].code == "E_PLAN_IO_DUPLICATE_RENDER_ID");
    REQUIRE_THAT(duplicate_render_result.error()[0].message,
                 ContainsSubstring("render_instructions"));

    auto duplicate_channel_render = json;
    duplicate_channel_render["channel_plans"][0]["render_instructions"].push_back(
        duplicate_channel_render["channel_plans"][0]["render_instructions"][0]);
    auto duplicate_channel_render_result = plan_from_json(duplicate_channel_render);
    REQUIRE_FALSE(duplicate_channel_render_result.has_value());
    REQUIRE(duplicate_channel_render_result.error()[0].code == "E_PLAN_IO_DUPLICATE_RENDER_ID");
    REQUIRE_THAT(duplicate_channel_render_result.error()[0].message,
                 ContainsSubstring("channel_plans.render_instructions"));

    auto duplicate_cross_channel_render = json;
    duplicate_cross_channel_render["normalized_scenario"]["channels"] = nlohmann::json::array({
        {{"id", "usrp0_ch0"},
         {"device", "usrp0"},
         {"index", 0},
         {"rf", duplicate_cross_channel_render["channels"][0]["rf"]}},
        {{"id", "usrp0_ch1"},
         {"device", "usrp0"},
         {"index", 1},
         {"rf", duplicate_cross_channel_render["channels"][0]["rf"]}}
    });
    duplicate_cross_channel_render["channels"].push_back(duplicate_cross_channel_render["channels"][0]);
    duplicate_cross_channel_render["channels"][1]["channel_index"] = 1;
    auto second_channel_plan = duplicate_cross_channel_render["channel_plans"][0];
    second_channel_plan["channel_id"] = "usrp0_ch1";
    second_channel_plan["channel_index"] = 1;
    duplicate_cross_channel_render["channel_plans"].push_back(second_channel_plan);
    auto duplicate_cross_channel_render_result = plan_from_json(duplicate_cross_channel_render);
    REQUIRE_FALSE(duplicate_cross_channel_render_result.has_value());
    REQUIRE(duplicate_cross_channel_render_result.error()[0].code ==
            "E_PLAN_IO_DUPLICATE_CHANNEL_PLAN_RENDER_ID");
    REQUIRE_THAT(duplicate_cross_channel_render_result.error()[0].message,
                 ContainsSubstring("multiple channel_plans"));

    auto duplicate_mix_member = json;
    duplicate_mix_member["mix_groups"] = nlohmann::json::array({
        {{"device_id", "usrp0"},
         {"channel", 0},
         {"start_sec", 0.0},
         {"duration_sec", 1.0},
         {"emitter_ids", nlohmann::json::array({"cw1", "cw1"})},
         {"estimated_peak_sum", 1.0}}
    });
    auto duplicate_mix_member_result = plan_from_json(duplicate_mix_member);
    REQUIRE_FALSE(duplicate_mix_member_result.has_value());
    REQUIRE(duplicate_mix_member_result.error()[0].code == "E_PLAN_IO_DUPLICATE_MIX_MEMBER");
    REQUIRE_THAT(duplicate_mix_member_result.error()[0].message,
                 ContainsSubstring("mix_groups.emitter_ids"));
}

TEST_CASE("Plan import rejects render instructions missing from channel plans", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());
    auto extra_render = json["render_instructions"][0];
    extra_render["emitter_id"] = "cw2";
    extra_render["start_sec"] = 10.0;
    json["render_instructions"].push_back(extra_render);

    auto result = plan_from_json(json);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_PLAN_IO_MISSING_CHANNEL_PLAN_RENDER_ID");
    REQUIRE_THAT(result.error()[0].message, ContainsSubstring("cw2"));
}

TEST_CASE("Plan import rejects unschedulable overlapping transmit windows", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    auto overlapping_render = json;
    auto second_render = overlapping_render["render_instructions"][0];
    second_render["emitter_id"] = "cw2";
    second_render["start_sec"] = 2.0;
    second_render["duration_sec"] = 1.0;
    overlapping_render["render_instructions"].push_back(second_render);
    overlapping_render["channel_plans"][0]["render_instructions"].push_back(second_render);
    auto overlapping_render_result = plan_from_json(overlapping_render);
    REQUIRE_FALSE(overlapping_render_result.has_value());
    REQUIRE(overlapping_render_result.error()[0].code == "E_PLAN_IO_OVERLAPPING_RENDER_INSTRUCTIONS");
    REQUIRE_THAT(overlapping_render_result.error()[0].message,
                 ContainsSubstring("without a covering mix group"));

    auto overlapping_mix_groups = json;
    overlapping_mix_groups["render_instructions"][0]["start_sec"] = 0.0;
    overlapping_mix_groups["render_instructions"][0]["duration_sec"] = 0.5;
    auto mix_member = overlapping_mix_groups["render_instructions"][0];
    mix_member["emitter_id"] = "cw2";
    mix_member["start_sec"] = 2.0;
    mix_member["duration_sec"] = 0.5;
    overlapping_mix_groups["render_instructions"].push_back(mix_member);
    overlapping_mix_groups["channel_plans"][0]["render_instructions"].push_back(mix_member);
    overlapping_mix_groups["mix_groups"] = nlohmann::json::array({
        {{"device_id", "usrp0"},
         {"channel", 0},
         {"start_sec", 0.0},
         {"duration_sec", 0.75},
         {"emitter_ids", nlohmann::json::array({"cw1"})},
         {"estimated_peak_sum", 0.5}},
        {{"device_id", "usrp0"},
         {"channel", 0},
         {"start_sec", 0.5},
         {"duration_sec", 0.75},
         {"emitter_ids", nlohmann::json::array({"cw2"})},
         {"estimated_peak_sum", 0.5}},
    });
    auto overlapping_mix_groups_result = plan_from_json(overlapping_mix_groups);
    REQUIRE_FALSE(overlapping_mix_groups_result.has_value());
    REQUIRE(overlapping_mix_groups_result.error()[0].code == "E_PLAN_IO_OVERLAPPING_MIX_GROUPS");

    auto mix_overlaps_render = json;
    mix_overlaps_render["render_instructions"][0]["start_sec"] = 0.0;
    mix_overlaps_render["render_instructions"][0]["duration_sec"] = 0.5;
    auto solo = mix_overlaps_render["render_instructions"][0];
    solo["emitter_id"] = "solo";
    solo["start_sec"] = 1.0;
    solo["duration_sec"] = 0.5;
    mix_overlaps_render["render_instructions"].push_back(solo);
    mix_overlaps_render["channel_plans"][0]["render_instructions"].push_back(solo);
    mix_overlaps_render["mix_groups"] = nlohmann::json::array({
        {{"device_id", "usrp0"},
         {"channel", 0},
         {"start_sec", 0.0},
         {"duration_sec", 1.25},
         {"emitter_ids", nlohmann::json::array({"cw1"})},
         {"estimated_peak_sum", 0.5}},
    });
    auto mix_overlaps_render_result = plan_from_json(mix_overlaps_render);
    REQUIRE_FALSE(mix_overlaps_render_result.has_value());
    REQUIRE(mix_overlaps_render_result.error()[0].code == "E_PLAN_IO_MIX_GROUP_OVERLAPS_RENDER");
    REQUIRE_THAT(mix_overlaps_render_result.error()[0].message,
                 ContainsSubstring("outside the group"));
}

TEST_CASE("Plan import rejects dangling execution references", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    auto dangling_channel_render = json;
    dangling_channel_render["channel_plans"][0]["render_instructions"][0]["emitter_id"] = "missing";
    auto dangling_channel_render_result = plan_from_json(dangling_channel_render);
    REQUIRE_FALSE(dangling_channel_render_result.has_value());
    REQUIRE(dangling_channel_render_result.error()[0].code == "E_PLAN_IO_DANGLING_RENDER_ID");
    REQUIRE_THAT(dangling_channel_render_result.error()[0].message,
                 ContainsSubstring("channel_plans"));

    auto dangling_channel_plan = json;
    dangling_channel_plan["channel_plans"][0]["channel_index"] = 99;
    auto dangling_channel_plan_result = plan_from_json(dangling_channel_plan);
    REQUIRE_FALSE(dangling_channel_plan_result.has_value());
    REQUIRE(dangling_channel_plan_result.error()[0].code == "E_PLAN_IO_DANGLING_CHANNEL_PLAN");
    REQUIRE_THAT(dangling_channel_plan_result.error()[0].message,
                 ContainsSubstring("channel_index"));

    auto dangling_top_level_channel = json;
    dangling_top_level_channel["channels"][0]["channel_index"] = 99;
    auto dangling_top_level_channel_result = plan_from_json(dangling_top_level_channel);
    REQUIRE_FALSE(dangling_top_level_channel_result.has_value());
    REQUIRE(dangling_top_level_channel_result.error()[0].code == "E_PLAN_IO_DANGLING_CHANNEL_BINDING");
    REQUIRE_THAT(dangling_top_level_channel_result.error()[0].message,
                 ContainsSubstring("normalized_scenario"));

    auto mismatched_channel_plan_id = json;
    mismatched_channel_plan_id["channel_plans"][0]["channel_id"] = "wrong";
    auto mismatched_channel_plan_id_result = plan_from_json(mismatched_channel_plan_id);
    REQUIRE_FALSE(mismatched_channel_plan_id_result.has_value());
    REQUIRE(mismatched_channel_plan_id_result.error()[0].code == "E_PLAN_IO_MISMATCHED_CHANNEL_PLAN_ID");
    REQUIRE_THAT(mismatched_channel_plan_id_result.error()[0].message,
                 ContainsSubstring("normalized_scenario"));

    auto dangling_mix_member = json;
    dangling_mix_member["mix_groups"] = nlohmann::json::array({
        {{"device_id", "usrp0"},
         {"channel", 0},
         {"start_sec", 0.0},
         {"duration_sec", 1.0},
         {"emitter_ids", nlohmann::json::array({"missing"})},
         {"estimated_peak_sum", 0.5}}
    });
    auto dangling_mix_member_result = plan_from_json(dangling_mix_member);
    REQUIRE_FALSE(dangling_mix_member_result.has_value());
    REQUIRE(dangling_mix_member_result.error()[0].code == "E_PLAN_IO_DANGLING_MIX_MEMBER");
    REQUIRE_THAT(dangling_mix_member_result.error()[0].message,
                 ContainsSubstring("mix_groups.emitter_ids"));

    auto dangling_mix_channel = json;
    dangling_mix_channel["mix_groups"] = nlohmann::json::array({
        {{"device_id", "usrp0"},
         {"channel", 99},
         {"start_sec", 0.0},
         {"duration_sec", 1.0},
         {"emitter_ids", nlohmann::json::array({"cw1"})},
         {"estimated_peak_sum", 0.5}}
    });
    auto dangling_mix_channel_result = plan_from_json(dangling_mix_channel);
    REQUIRE_FALSE(dangling_mix_channel_result.has_value());
    REQUIRE(dangling_mix_channel_result.error()[0].code == "E_PLAN_IO_DANGLING_MIX_CHANNEL");
    REQUIRE_THAT(dangling_mix_channel_result.error()[0].message,
                 ContainsSubstring("device/channel"));
}

TEST_CASE("Plan import rejects dangling timeline targets", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    auto dangling_emitter_start = json;
    dangling_emitter_start["timeline"][0]["target_id"] = "missing";
    auto dangling_emitter_start_result = plan_from_json(dangling_emitter_start);
    REQUIRE_FALSE(dangling_emitter_start_result.has_value());
    REQUIRE(dangling_emitter_start_result.error()[0].code == "E_PLAN_IO_DANGLING_TIMELINE_TARGET");
    REQUIRE_THAT(dangling_emitter_start_result.error()[0].message,
                 ContainsSubstring("render_instructions"));

    auto dangling_device_event = json;
    dangling_device_event["timeline"].push_back({
        {"type", "FreqChange"},
        {"time_sec", 0.0},
        {"target_id", "missing_device"},
        {"payload", {{"freq_hz", 1.1e9}}},
    });
    auto dangling_device_event_result = plan_from_json(dangling_device_event);
    REQUIRE_FALSE(dangling_device_event_result.has_value());
    REQUIRE(dangling_device_event_result.error()[0].code == "E_PLAN_IO_DANGLING_TIMELINE_TARGET");
    REQUIRE_THAT(dangling_device_event_result.error()[0].message,
                 ContainsSubstring("channels"));

    auto dangling_event_channel = json;
    dangling_event_channel["timeline"].push_back({
        {"type", "GainChange"},
        {"time_sec", 0.0},
        {"target_id", "usrp0"},
        {"payload", {{"gain_db", 12.0}, {"channel", 99}}},
    });
    auto dangling_event_channel_result = plan_from_json(dangling_event_channel);
    REQUIRE_FALSE(dangling_event_channel_result.has_value());
    REQUIRE(dangling_event_channel_result.error()[0].code == "E_PLAN_IO_DANGLING_TIMELINE_CHANNEL");
    REQUIRE_THAT(dangling_event_channel_result.error()[0].message,
                 ContainsSubstring("payload.channel"));

    auto invalid_event_channel = json;
    invalid_event_channel["timeline"].push_back({
        {"type", "GainChange"},
        {"time_sec", 0.0},
        {"target_id", "usrp0"},
        {"payload", {{"gain_db", 12.0}, {"channel", -1}}},
    });
    auto invalid_event_channel_result = plan_from_json(invalid_event_channel);
    REQUIRE_FALSE(invalid_event_channel_result.has_value());
    REQUIRE(invalid_event_channel_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
    REQUIRE_THAT(invalid_event_channel_result.error()[0].message,
                 ContainsSubstring("timeline.payload.channel"));

    auto dangling_waveform_switch_emitter = json;
    dangling_waveform_switch_emitter["timeline"].push_back({
        {"type", "WaveformSwitch"},
        {"time_sec", 0.0},
        {"target_id", "usrp0"},
        {"payload", {{"emitter_id", "missing"}, {"new_waveform", "wf2"}}},
    });
    auto dangling_waveform_switch_result = plan_from_json(dangling_waveform_switch_emitter);
    REQUIRE_FALSE(dangling_waveform_switch_result.has_value());
    REQUIRE(dangling_waveform_switch_result.error()[0].code ==
            "E_PLAN_IO_DANGLING_TIMELINE_PAYLOAD_EMITTER");
    REQUIRE_THAT(dangling_waveform_switch_result.error()[0].message,
                 ContainsSubstring("payload.emitter_id"));

    auto dangling_impairment_change_emitter = json;
    dangling_impairment_change_emitter["timeline"].push_back({
        {"type", "ImpairmentChange"},
        {"time_sec", 0.0},
        {"target_id", "usrp0"},
        {"payload", {{"emitter_id", "missing"}, {"impairment", "cfo_hz"}}},
    });
    auto dangling_impairment_change_result = plan_from_json(dangling_impairment_change_emitter);
    REQUIRE_FALSE(dangling_impairment_change_result.has_value());
    REQUIRE(dangling_impairment_change_result.error()[0].code ==
            "E_PLAN_IO_DANGLING_TIMELINE_PAYLOAD_EMITTER");
    REQUIRE_THAT(dangling_impairment_change_result.error()[0].message,
                 ContainsSubstring("payload.emitter_id"));
}

TEST_CASE("Plan import rejects malformed timeline payloads", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("payload must be an object when present") {
        auto bad_payload = json;
        bad_payload["timeline"].push_back({
            {"type", "Marker"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", "not an object"},
        });
        auto result = plan_from_json(bad_payload);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("timeline.payload"));
    }

    SECTION("frequency change requires positive freq_hz") {
        auto missing_freq = json;
        missing_freq["timeline"].push_back({
            {"type", "FreqChange"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", nlohmann::json::object()},
        });
        auto missing_result = plan_from_json(missing_freq);
        REQUIRE_FALSE(missing_result.has_value());
        REQUIRE(missing_result.error()[0].code == "E_PLAN_IO_BAD_TIMELINE_PAYLOAD");
        REQUIRE_THAT(missing_result.error()[0].message, ContainsSubstring("freq_hz"));

        auto invalid_freq = json;
        invalid_freq["timeline"].push_back({
            {"type", "FreqChange"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", {{"freq_hz", 0.0}}},
        });
        auto invalid_result = plan_from_json(invalid_freq);
        REQUIRE_FALSE(invalid_result.has_value());
        REQUIRE(invalid_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(invalid_result.error()[0].message, ContainsSubstring("timeline.payload.freq_hz"));
    }

    SECTION("gain change requires finite gain_db") {
        auto invalid_gain = json;
        invalid_gain["timeline"].push_back({
            {"type", "GainChange"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", {{"gain_db", "high"}}},
        });
        auto result = plan_from_json(invalid_gain);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("timeline.payload.gain_db"));
    }

    SECTION("waveform switch requires string payload fields") {
        auto missing_waveform = json;
        missing_waveform["timeline"].push_back({
            {"type", "WaveformSwitch"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", {{"emitter_id", "cw1"}}},
        });
        auto result = plan_from_json(missing_waveform);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_BAD_TIMELINE_PAYLOAD");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("new_waveform"));
    }

    SECTION("marker payload name must be non-empty") {
        auto empty_marker = json;
        empty_marker["timeline"].push_back({
            {"type", "Marker"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", {{"name", ""}}},
        });
        auto result = plan_from_json(empty_marker);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("timeline.payload.name"));
    }

    SECTION("waveform switch payload strings must be non-empty") {
        auto empty_waveform = json;
        empty_waveform["timeline"].push_back({
            {"type", "WaveformSwitch"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", {{"emitter_id", "cw1"}, {"new_waveform", ""}}},
        });
        auto result = plan_from_json(empty_waveform);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("timeline.payload.new_waveform"));
    }

    SECTION("impairment change validates optional enabled type") {
        auto invalid_enabled = json;
        invalid_enabled["timeline"].push_back({
            {"type", "ImpairmentChange"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", {{"emitter_id", "cw1"}, {"impairment", "cfo_hz"}, {"enabled", "yes"}}},
        });
        auto result = plan_from_json(invalid_enabled);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("timeline.payload.enabled"));
    }

    SECTION("impairment change payload strings must be non-empty") {
        auto empty_impairment = json;
        empty_impairment["timeline"].push_back({
            {"type", "ImpairmentChange"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", {{"emitter_id", "cw1"}, {"impairment", ""}}},
        });
        auto result = plan_from_json(empty_impairment);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("timeline.payload.impairment"));
    }
}

TEST_CASE("Plan import rejects dangling channel plan events", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("emitter event must target a render instruction in the same channel plan") {
        auto dangling_emitter = json;
        dangling_emitter["channel_plans"][0]["events"][0]["target_id"] = "missing";
        auto result = plan_from_json(dangling_emitter);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_DANGLING_CHANNEL_PLAN_EVENT_TARGET");
        REQUIRE_THAT(result.error()[0].message,
                     ContainsSubstring("channel_plans.render_instructions"));
    }

    SECTION("device event must target a device bound to the channel plan index") {
        auto dangling_device = json;
        dangling_device["channel_plans"][0]["events"].push_back({
            {"type", "FreqChange"},
            {"time_sec", 0.0},
            {"target_id", "missing_device"},
            {"payload", {{"freq_hz", 1.1e9}}},
        });
        auto result = plan_from_json(dangling_device);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_DANGLING_CHANNEL_PLAN_EVENT_TARGET");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("channel_plans.channel_index"));
    }

    SECTION("event payload channel must match the channel plan index") {
        auto wrong_channel = json;
        wrong_channel["channel_plans"][0]["events"].push_back({
            {"type", "GainChange"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", {{"gain_db", 12.0}, {"channel", 99}}},
        });
        auto result = plan_from_json(wrong_channel);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_DANGLING_CHANNEL_PLAN_EVENT_CHANNEL");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("payload.channel"));
    }

    SECTION("event payload shape is validated inside channel plans") {
        auto missing_gain = json;
        missing_gain["channel_plans"][0]["events"].push_back({
            {"type", "GainChange"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", nlohmann::json::object()},
        });
        auto result = plan_from_json(missing_gain);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_BAD_TIMELINE_PAYLOAD");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("gain_db"));
    }

    SECTION("event payload emitter must belong to the channel plan") {
        auto dangling_payload_emitter = json;
        dangling_payload_emitter["channel_plans"][0]["events"].push_back({
            {"type", "WaveformSwitch"},
            {"time_sec", 0.0},
            {"target_id", "usrp0"},
            {"payload", {{"emitter_id", "missing"}, {"new_waveform", "wf2"}}},
        });
        auto result = plan_from_json(dangling_payload_emitter);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_PLAN_IO_DANGLING_CHANNEL_PLAN_EVENT_PAYLOAD_EMITTER");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("payload.emitter_id"));
    }
}

TEST_CASE("Render instruction impairments round-trip", "[plan_io]") {
    auto p = make_simple_plan();
    ImpairmentSettings imp;
    imp.cfo_hz = 250.0;
    imp.phase_offset_rad = 0.25;
    p.render_instructions[0].impairments = imp;
    p.channel_plans[0].render_instructions[0].impairments = imp;

    auto json = plan_to_json(p);
    REQUIRE(json["render_instructions"][0]["impairments"]["cfo_hz"].get<double>() == 250.0);

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result->render_instructions[0].impairments.has_value());
    REQUIRE(restored_result->render_instructions[0].impairments->cfo_hz == 250.0);
    REQUIRE(restored_result->channel_plans[0].render_instructions[0].impairments.has_value());
    REQUIRE(restored_result->channel_plans[0].render_instructions[0].impairments->phase_offset_rad == 0.25);
}

TEST_CASE("Resource estimates and channel plan events round-trip", "[plan_io]") {
    auto p = make_simple_plan();
    REQUIRE_FALSE(p.channel_plans.empty());
    REQUIRE_FALSE(p.channel_plans[0].events.empty());

    auto json = plan_to_json(p);
    REQUIRE(json.contains("resource_estimate"));
    REQUIRE(json["channel_plans"][0].contains("events"));
    REQUIRE(json["channel_plans"][0].contains("resource_estimate"));

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result->resource_estimate.peak_memory_bytes == p.resource_estimate.peak_memory_bytes);
    REQUIRE(restored_result->resource_estimate.timing_feasible == p.resource_estimate.timing_feasible);
    REQUIRE(restored_result->channel_plans[0].events.size() == p.channel_plans[0].events.size());
    REQUIRE(restored_result->channel_plans[0].events[0].type == p.channel_plans[0].events[0].type);
    REQUIRE_THAT(restored_result->channel_plans[0].events[0].time_sec,
                 WithinAbs(p.channel_plans[0].events[0].time_sec, 1e-12));
}

TEST_CASE("Plan JSON rejects invalid enum strings", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("top-level run_mode") {
        json["run_mode"] = "buffered";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_RUN_MODE");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("buffered"));
    }

    SECTION("normalized scenario run mode") {
        json["normalized_scenario"]["run"]["mode"] = "buffered";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_RUN_MODE");
    }

    SECTION("normalized scenario emitter mixing") {
        json["normalized_scenario"]["emitters"][0]["mixing"] = "parallel";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_MIXING_MODE");
    }
}

TEST_CASE("Plan JSON type errors are returned as structured errors", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());
    json["channels"][0]["channel_index"] = "zero";

    auto restored_result = plan_from_json(json);
    REQUIRE_FALSE(restored_result.has_value());
    REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
    REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("type"));
}

TEST_CASE("Plan JSON rejects empty top-level identity strings", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("render instruction emitter id") {
        json["render_instructions"][0]["emitter_id"] = "";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_EMPTY_RENDER_ID");
    }

    SECTION("channel device id") {
        json["channels"][0]["device_id"] = "";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_EMPTY_CHANNEL_DEVICE_ID");
    }

    SECTION("channel plan id") {
        json["channel_plans"][0]["channel_id"] = "";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_EMPTY_CHANNEL_PLAN_ID");
    }

    SECTION("mix group device id") {
        json["mix_groups"] = nlohmann::json::array({
            {{"device_id", ""},
             {"channel", 0},
             {"start_sec", 6.0},
             {"duration_sec", 1.0},
             {"emitter_ids", nlohmann::json::array({"cw1"})},
             {"estimated_peak_sum", 0.5}}
        });
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_EMPTY_MIX_DEVICE_ID");
    }

    SECTION("mix group member id") {
        json["mix_groups"] = nlohmann::json::array({
            {{"device_id", "usrp0"},
             {"channel", 0},
             {"start_sec", 6.0},
             {"duration_sec", 1.0},
             {"emitter_ids", nlohmann::json::array({""})},
             {"estimated_peak_sum", 0.5}}
        });
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_EMPTY_MIX_MEMBER");
    }
}

TEST_CASE("Plan JSON rejects invalid scalar field types with field names", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("timeline type must be string") {
        json["timeline"][0]["type"] = 42;
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("timeline.type"));
    }

    SECTION("timeline time must be finite number") {
        json["timeline"][0]["time_sec"] = "soon";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("timeline.time_sec"));
    }

    SECTION("render instruction emitter id must be string") {
        json["render_instructions"][0]["emitter_id"] = false;
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("render_instructions.emitter_id"));
    }

    SECTION("render instruction sample rate must be finite number") {
        json["render_instructions"][0]["sample_rate"] = "fast";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("render_instructions.sample_rate"));
    }

    SECTION("resource estimate timing feasible must be boolean") {
        json["resource_estimate"]["timing_feasible"] = "yes";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("resource_estimate.timing_feasible"));
    }

    SECTION("normalized scenario RF frequency must be finite number") {
        json["normalized_scenario"]["devices"][0]["rf"]["freq_hz"] = "2.4GHz";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("rf.freq_hz"));
    }

    SECTION("normalized scenario waveform target power must be finite number") {
        json["normalized_scenario"]["emitters"][0]["waveform"]["target_power_dbm"] = "low";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("waveform.target_power_dbm"));
    }

    SECTION("normalized scenario reporting flag must be boolean") {
        json["normalized_scenario"]["reporting"]["save_plan"] = "true";
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("reporting.save_plan"));
    }

    SECTION("normalized scenario event target must be string") {
        json["normalized_scenario"]["events"] = nlohmann::json::array({
            {{"target_device", 0}, {"time_sec", 1.0}, {"type", "marker"}}
        });
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("events.target_device"));
    }

    SECTION("normalized scenario event payload must be object") {
        json["normalized_scenario"]["events"] = nlohmann::json::array({
            {{"target_device", "usrp0"}, {"time_sec", 1.0}, {"type", "marker"}, {"payload", "mark"}}
        });
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("events.payload"));
    }

    SECTION("normalized scenario impairment value must be finite number") {
        json["normalized_scenario"]["emitters"][0]["impairments"] = {{"cfo_hz", "fast"}};
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("impairments.cfo_hz"));
    }

    SECTION("mix group device id must be string") {
        json["mix_groups"] = nlohmann::json::array({
            {{"device_id", 42},
             {"channel", 0},
             {"start_sec", 0.0},
             {"duration_sec", 1.0},
             {"emitter_ids", nlohmann::json::array({"cw1"})},
             {"estimated_peak_sum", 0.5}}
        });
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("mix_groups.device_id"));
    }

    SECTION("run mode must be string") {
        json["run_mode"] = false;
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("run_mode"));
    }
}

TEST_CASE("Plan JSON rejects invalid unsigned channel fields", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("normalized scenario device channel is negative") {
        json["normalized_scenario"]["devices"][0]["channel"] = -1;
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("device.channel"));
    }

    SECTION("normalized scenario emitter channel exceeds uint32") {
        json["normalized_scenario"]["emitters"][0]["channel"] = 4294967296;
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("emitter.channel"));
    }

    SECTION("normalized scenario channel definition index is non-integer") {
        json["normalized_scenario"]["channels"] = nlohmann::json::array({
            {{"id", "ch0"}, {"device", "usrp0"}, {"index", 1.5}, {"rf", json["channels"][0]["rf"]}}
        });
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("channel.index"));
    }

    SECTION("plan channel binding index is negative") {
        json["channels"][0]["channel_index"] = -1;
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("channels.channel_index"));
    }

    SECTION("mix group channel exceeds uint32") {
        json["mix_groups"] = nlohmann::json::array({
            {{"device_id", "usrp0"},
             {"channel", 4294967296},
             {"start_sec", 0.0},
             {"duration_sec", 1.0},
             {"emitter_ids", nlohmann::json::array({"cw1"})},
             {"estimated_peak_sum", 0.5}}
        });
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("mix_groups.channel"));
    }

    SECTION("channel plan index is non-integer") {
        json["channel_plans"][0]["channel_index"] = 1.5;
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("channel_plans.channel_index"));
    }

    SECTION("resource estimate peak memory is negative") {
        json["resource_estimate"]["peak_memory_bytes"] = -1;
        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("resource_estimate.peak_memory_bytes"));
    }
}

TEST_CASE("Plan JSON rejects invalid repeat counts", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("zero normalized scenario repeat count") {
        json["normalized_scenario"]["emitters"][0]["repeat"] = {{"count", 0}, {"interval_sec", 1.0}};

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "V017_INVALID_REPEAT_COUNT");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("repeat count"));
    }

    SECTION("fractional normalized scenario repeat count") {
        json["normalized_scenario"]["emitters"][0]["repeat"] = {{"count", 1.5}, {"interval_sec", 1.0}};

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("emitter.repeat.count"));
    }

    SECTION("normalized scenario repeat count above planner maximum") {
        json["normalized_scenario"]["emitters"][0]["repeat"] = {{"count", 1025}, {"interval_sec", 1.0}};

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "V017_INVALID_REPEAT_COUNT");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("repeat count"));
    }

    SECTION("out-of-range normalized scenario repeat count") {
        json["normalized_scenario"]["emitters"][0]["repeat"] = {{"count", 2147483648}, {"interval_sec", 1.0}};

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("emitter.repeat.count"));
    }

    SECTION("normalized scenario multiple repeats require positive interval") {
        json["normalized_scenario"]["emitters"][0]["repeat"] = {{"count", 2}, {"interval_sec", 0.0}};

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "V018_INVALID_REPEAT_INTERVAL");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("repeat interval_sec"));
    }
}

TEST_CASE("Plan import rejects semantically invalid normalized scenarios", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("zero normalized scenario emitter duration") {
        json["normalized_scenario"]["emitters"][0]["duration_sec"] = 0.0;

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "V002_INVALID_DURATION");
    }
}

TEST_CASE("Plan JSON rejects invalid string arrays", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("resource estimate warnings must be an array") {
        json["resource_estimate"]["warnings"] = "late";

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("resource_estimate.warnings"));
    }

    SECTION("resource estimate warning entries must be strings") {
        json["resource_estimate"]["warnings"] = nlohmann::json::array({42});

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("resource_estimate.warnings"));
    }

    SECTION("mix group emitter ids must be an array") {
        json["mix_groups"] = nlohmann::json::array({
            {{"device_id", "usrp0"},
             {"channel", 0},
             {"start_sec", 0.0},
             {"duration_sec", 1.0},
             {"emitter_ids", "cw1"},
             {"estimated_peak_sum", 0.5}}
        });

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("mix_groups.emitter_ids"));
    }

    SECTION("mix group emitter id entries must be strings") {
        json["mix_groups"] = nlohmann::json::array({
            {{"device_id", "usrp0"},
             {"channel", 0},
             {"start_sec", 0.0},
             {"duration_sec", 1.0},
             {"emitter_ids", nlohmann::json::array({42})},
             {"estimated_peak_sum", 0.5}}
        });

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("mix_groups.emitter_ids"));
    }

    SECTION("sync group channels must be an array") {
        json["normalized_scenario"]["sync_groups"] = nlohmann::json::array({
            {{"id", "sg0"}, {"channels", "ch0"}, {"mode", "coherent"}}
        });

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("sync_groups.channels"));
    }

    SECTION("sync group channel entries must be strings") {
        json["normalized_scenario"]["sync_groups"] = nlohmann::json::array({
            {{"id", "sg0"}, {"channels", nlohmann::json::array({0})}, {"mode", "coherent"}}
        });

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("sync_groups.channels"));
    }

    SECTION("sync group mode must be supported") {
        json["normalized_scenario"]["sync_groups"] = nlohmann::json::array({
            {{"id", "sg0"}, {"channels", nlohmann::json::array({"ch0"})}, {"mode", "timed"}}
        });

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_SYNC_MODE");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("timed"));
    }
}

TEST_CASE("Plan JSON rejects invalid repeated section containers", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    SECTION("top-level plan must be an object") {
        auto restored_result = plan_from_json(nlohmann::json::array());
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("plan"));
    }

    SECTION("top-level channels must be an array") {
        json["channels"] = nlohmann::json::object({{"ch0", json["channels"][0]}});

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("channels"));
    }

    SECTION("top-level channel entries must be objects") {
        json["channels"] = nlohmann::json::array({42});

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("channel"));
    }

    SECTION("normalized scenario devices must be an array") {
        json["normalized_scenario"]["devices"] =
            nlohmann::json::object({{"usrp0", json["normalized_scenario"]["devices"][0]}});

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("devices"));
    }

    SECTION("normalized scenario device entries must be objects") {
        json["normalized_scenario"]["devices"] = nlohmann::json::array({"usrp0"});

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("device"));
    }

    SECTION("normalized scenario device RF must be an object") {
        json["normalized_scenario"]["devices"][0]["rf"] = "rf-profile";

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("rf"));
    }

    SECTION("normalized scenario emitter repeat must be an object") {
        json["normalized_scenario"]["emitters"][0]["repeat"] = "forever";

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("emitter.repeat"));
    }

    SECTION("normalized scenario run must be an object") {
        json["normalized_scenario"]["run"] = "replay";

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("run"));
    }

    SECTION("top-level timeline entries must be objects") {
        json["timeline"] = nlohmann::json::array({"start"});

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("timeline"));
    }

    SECTION("top-level render instruction entries must be objects") {
        json["render_instructions"] = nlohmann::json::array({"cw1"});

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("render_instructions"));
    }

    SECTION("channel plan render instructions must be an array") {
        json["channel_plans"][0]["render_instructions"] =
            nlohmann::json::object({{"ri0", json["channel_plans"][0]["render_instructions"][0]}});

        auto restored_result = plan_from_json(json);
        REQUIRE_FALSE(restored_result.has_value());
        REQUIRE(restored_result.error()[0].code == "E_PLAN_IO_BAD_JSON");
        REQUIRE_THAT(restored_result.error()[0].message, ContainsSubstring("channel_plans.render_instructions"));
    }
}

TEST_CASE("Normalized scenario round-trips through plan JSON", "[plan_io]") {
    auto original = make_simple_plan();
    auto json = plan_to_json(original);

    REQUIRE(json["normalized_scenario"]["metadata"]["name"].get<std::string>() == "test_io");
    REQUIRE(json["normalized_scenario"]["devices"].size() == 1);
    REQUIRE(json["normalized_scenario"]["emitters"].size() == 1);

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().normalized_scenario.metadata.name == "test_io");
    REQUIRE(restored_result.value().normalized_scenario.devices.size() == 1);
    REQUIRE_THAT(restored_result.value().normalized_scenario.devices[0].rf.freq_hz,
                 WithinAbs(2.45e9, 1.0));
}

TEST_CASE("Channel bindings with optional RF fields round-trip", "[plan_io]") {
    auto original = make_simple_plan();
    REQUIRE(original.channels[0].rf.bandwidth_hz.has_value());
    REQUIRE(original.channels[0].rf.antenna.has_value());

    auto json = plan_to_json(original);
    REQUIRE(json["channels"][0]["rf"].contains("bandwidth_hz"));
    REQUIRE(json["channels"][0]["rf"].contains("antenna"));

    auto restored_result = plan_from_json(json);
    REQUIRE(restored_result.has_value());
    REQUIRE(restored_result.value().channels[0].rf.bandwidth_hz.has_value());
    REQUIRE_THAT(*restored_result.value().channels[0].rf.bandwidth_hz, WithinAbs(8e6, 1.0));
    REQUIRE(restored_result.value().channels[0].rf.antenna.has_value());
    REQUIRE(*restored_result.value().channels[0].rf.antenna == "TX/RX");
}

TEST_CASE("Plan import rejects missing required RF fields", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    auto missing_device_rf = json;
    missing_device_rf["normalized_scenario"]["devices"][0].erase("rf");
    CHECK_FALSE(plan_from_json(missing_device_rf).has_value());

    auto missing_channel_rf = json;
    missing_channel_rf["channels"][0].erase("rf");
    CHECK_FALSE(plan_from_json(missing_channel_rf).has_value());

    auto missing_channel_plan_rate = json;
    missing_channel_plan_rate["channel_plans"][0]["rf"].erase("rate_sps");
    CHECK_FALSE(plan_from_json(missing_channel_plan_rate).has_value());

    auto missing_rf_gain = json;
    missing_rf_gain["channels"][0]["rf"].erase("gain_db");
    CHECK_FALSE(plan_from_json(missing_rf_gain).has_value());
}

TEST_CASE("Plan import rejects missing required event and render fields", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    auto missing_timeline_type = json;
    missing_timeline_type["timeline"][0].erase("type");
    CHECK_FALSE(plan_from_json(missing_timeline_type).has_value());

    auto missing_timeline_target = json;
    missing_timeline_target["timeline"][0].erase("target_id");
    CHECK_FALSE(plan_from_json(missing_timeline_target).has_value());

    auto missing_waveform_type = json;
    missing_waveform_type["render_instructions"][0]["waveform"].erase("type");
    CHECK_FALSE(plan_from_json(missing_waveform_type).has_value());

    auto missing_render_rate = json;
    missing_render_rate["render_instructions"][0].erase("sample_rate");
    CHECK_FALSE(plan_from_json(missing_render_rate).has_value());

    auto missing_channel_plan_event_type = json;
    missing_channel_plan_event_type["channel_plans"][0]["events"][0].erase("type");
    CHECK_FALSE(plan_from_json(missing_channel_plan_event_type).has_value());
}

TEST_CASE("Plan import rejects missing required execution artifact fields", "[plan_io]") {
    auto json = plan_to_json(make_simple_plan());

    auto missing_channel_device = json;
    missing_channel_device["channels"][0].erase("device_id");
    CHECK_FALSE(plan_from_json(missing_channel_device).has_value());

    auto missing_channel_index = json;
    missing_channel_index["channels"][0].erase("channel_index");
    CHECK_FALSE(plan_from_json(missing_channel_index).has_value());

    auto missing_mix_emitters = json;
    missing_mix_emitters["mix_groups"] = nlohmann::json::array({
        {{"device_id", "usrp0"},
         {"channel", 0},
         {"start_sec", 0.0},
         {"duration_sec", 1.0},
         {"emitter_ids", nlohmann::json::array({"cw1"})},
         {"estimated_peak_sum", 0.5}}
    });
    missing_mix_emitters["mix_groups"][0].erase("emitter_ids");
    CHECK_FALSE(plan_from_json(missing_mix_emitters).has_value());

    auto missing_mix_peak = json;
    missing_mix_peak["mix_groups"] = nlohmann::json::array({
        {{"device_id", "usrp0"},
         {"channel", 0},
         {"start_sec", 0.0},
         {"duration_sec", 1.0},
         {"emitter_ids", nlohmann::json::array({"cw1"})},
         {"estimated_peak_sum", 0.5}}
    });
    missing_mix_peak["mix_groups"][0].erase("estimated_peak_sum");
    CHECK_FALSE(plan_from_json(missing_mix_peak).has_value());

    auto missing_resource_warnings = json;
    missing_resource_warnings["resource_estimate"].erase("warnings");
    CHECK_FALSE(plan_from_json(missing_resource_warnings).has_value());

    auto missing_channel_plan_id = json;
    missing_channel_plan_id["channel_plans"][0].erase("channel_id");
    CHECK_FALSE(plan_from_json(missing_channel_plan_id).has_value());

    auto missing_channel_plan_events = json;
    missing_channel_plan_events["channel_plans"][0].erase("events");
    CHECK_FALSE(plan_from_json(missing_channel_plan_events).has_value());

    auto missing_channel_plan_estimate = json;
    missing_channel_plan_estimate["channel_plans"][0].erase("resource_estimate");
    CHECK_FALSE(plan_from_json(missing_channel_plan_estimate).has_value());
}

TEST_CASE("Plan import rejects missing required top-level fields", "[plan_io]") {
    const auto json = plan_to_json(make_simple_plan());

    for (const std::string key : {"normalized_scenario", "channels", "timeline", "render_instructions",
                                  "warnings", "estimated_duration_sec", "resource_estimate", "mix_groups",
                                  "channel_plans", "run_mode"}) {
        auto missing = json;
        missing.erase(key);
        CHECK_FALSE(plan_from_json(missing).has_value());
    }
}

TEST_CASE("Plan import rejects missing required normalized scenario fields", "[plan_io]") {
    const auto json = plan_to_json(make_simple_plan());

    for (const auto& path : {
             std::vector<std::string>{"metadata"},
             std::vector<std::string>{"metadata", "name"},
             std::vector<std::string>{"devices"},
             std::vector<std::string>{"devices", "0", "id"},
             std::vector<std::string>{"emitters"},
             std::vector<std::string>{"emitters", "0", "id"},
             std::vector<std::string>{"emitters", "0", "device"},
             std::vector<std::string>{"emitters", "0", "channel"},
             std::vector<std::string>{"emitters", "0", "start_after_sec"},
             std::vector<std::string>{"emitters", "0", "duration_sec"},
             std::vector<std::string>{"emitters", "0", "mixing"},
             std::vector<std::string>{"reporting"},
             std::vector<std::string>{"reporting", "save_plan"},
             std::vector<std::string>{"reporting", "save_metrics"},
             std::vector<std::string>{"run"},
             std::vector<std::string>{"run", "mode"},
         }) {
        auto missing = json;
        auto* current = &missing["normalized_scenario"];
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            current = path[i] == "0" ? &(*current)[0] : &(*current)[path[i]];
        }
        current->erase(path.back());
        CHECK_FALSE(plan_from_json(missing).has_value());
    }
}
