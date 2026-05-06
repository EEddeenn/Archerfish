#include "archerfish/scenario/parser.hpp"
#include "archerfish/dsp/waveform_type.hpp"
using archerfish::dsp::WaveformType;

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/planner.hpp"

#include <algorithm>
#include <limits>

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;

namespace {

Scenario make_simple_scenario() {
    Scenario s;
    s.metadata.name = "test_simple";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});

    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 1.0;
    em.duration_sec = 5.0;
    em.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em);

    return s;
}

Scenario make_multi_emitter_same_device_scenario() {
    Scenario s;
    s.metadata.name = "multi_same_device";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});

    EmitterDef em1;
    em1.id = "cw1";
    em1.device = "usrp0";
    em1.channel = 0;
    em1.start_after_sec = 1.0;
    em1.duration_sec = 1.0;
    em1.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em1);

    EmitterDef em2;
    em2.id = "cw2";
    em2.device = "usrp0";
    em2.channel = 0;
    em2.start_after_sec = 2.0;
    em2.duration_sec = 4.0;
    em2.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.3}}};
    s.emitters.push_back(em2);

    return s;
}

Scenario make_multi_device_scenario() {
    Scenario s;
    s.metadata.name = "multi_device";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
    s.devices.push_back({"usrp1", 0, {915e6, 20e6, 18.0}});

    EmitterDef em1;
    em1.id = "cw1";
    em1.device = "usrp0";
    em1.channel = 0;
    em1.start_after_sec = 1.0;
    em1.duration_sec = 5.0;
    em1.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em1);

    EmitterDef em2;
    em2.id = "cw2";
    em2.device = "usrp1";
    em2.channel = 0;
    em2.start_after_sec = 0.5;
    em2.duration_sec = 3.0;
    em2.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.3}}};
    s.emitters.push_back(em2);

    return s;
}

} // namespace

TEST_CASE("Single emitter produces correct channel bindings", "[planner]") {
    auto result = plan(make_simple_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.channels.size() == 1);
    REQUIRE(p.channels[0].device_id == "usrp0");
    REQUIRE(p.channels[0].channel_index == 0);
    REQUIRE_THAT(p.channels[0].rf.freq_hz, WithinAbs(2.45e9, 1.0));
    REQUIRE_THAT(p.channels[0].rf.rate_sps, WithinAbs(10e6, 1.0));
    REQUIRE_THAT(p.channels[0].rf.gain_db, WithinAbs(20.0, 1e-12));
}

TEST_CASE("Single emitter produces 2 timeline events (start + stop)", "[planner]") {
    auto result = plan(make_simple_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.timeline.size() == 2);
    REQUIRE(p.timeline[0].type == TimelineEventType::EmitterStart);
    REQUIRE_THAT(p.timeline[0].time_sec, WithinAbs(1.0, 1e-12));
    REQUIRE(p.timeline[0].target_id == "cw1");

    REQUIRE(p.timeline[1].type == TimelineEventType::EmitterStop);
    REQUIRE_THAT(p.timeline[1].time_sec, WithinAbs(6.0, 1e-12));
    REQUIRE(p.timeline[1].target_id == "cw1");
}

TEST_CASE("Single emitter produces 1 render instruction", "[planner]") {
    auto result = plan(make_simple_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.render_instructions.size() == 1);
    REQUIRE(p.render_instructions[0].emitter_id == "cw1");
    REQUIRE(p.render_instructions[0].waveform.type == WaveformType::CW);
    REQUIRE_THAT(p.render_instructions[0].start_sec, WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(p.render_instructions[0].duration_sec, WithinAbs(5.0, 1e-12));
    REQUIRE_THAT(p.render_instructions[0].sample_rate, WithinAbs(10e6, 1.0));
    REQUIRE_FALSE(p.render_instructions[0].resample_ratio.has_value());
}

TEST_CASE("Multiple emitters on same device produce sorted timeline", "[planner]") {
    auto result = plan(make_multi_emitter_same_device_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.timeline.size() == 4);

    REQUIRE(p.timeline[0].type == TimelineEventType::EmitterStart);
    REQUIRE_THAT(p.timeline[0].time_sec, WithinAbs(1.0, 1e-12));
    REQUIRE(p.timeline[0].target_id == "cw1");

    REQUIRE(p.timeline[1].type == TimelineEventType::EmitterStop);
    REQUIRE_THAT(p.timeline[1].time_sec, WithinAbs(2.0, 1e-12));
    REQUIRE(p.timeline[1].target_id == "cw1");

    REQUIRE(p.timeline[2].type == TimelineEventType::EmitterStart);
    REQUIRE_THAT(p.timeline[2].time_sec, WithinAbs(2.0, 1e-12));
    REQUIRE(p.timeline[2].target_id == "cw2");

    REQUIRE(p.timeline[3].type == TimelineEventType::EmitterStop);
    REQUIRE_THAT(p.timeline[3].time_sec, WithinAbs(6.0, 1e-12));
    REQUIRE(p.timeline[3].target_id == "cw2");
}

TEST_CASE("Multiple devices produce separate channel bindings", "[planner]") {
    auto result = plan(make_multi_device_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.channels.size() == 2);
    REQUIRE(p.channels[0].device_id == "usrp0");
    REQUIRE_THAT(p.channels[0].rf.rate_sps, WithinAbs(10e6, 1.0));
    REQUIRE(p.channels[1].device_id == "usrp1");
    REQUIRE_THAT(p.channels[1].rf.rate_sps, WithinAbs(20e6, 1.0));
}

TEST_CASE("Estimated duration equals max start + duration", "[planner]") {
    auto result = plan(make_multi_emitter_same_device_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE_THAT(p.estimated_duration_sec, WithinAbs(6.0, 1e-12));
}

TEST_CASE("Sample rate from device RF settings flows into render instructions", "[planner]") {
    auto result = plan(make_multi_device_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.render_instructions.size() == 2);
    REQUIRE_THAT(p.render_instructions[0].sample_rate, WithinAbs(10e6, 1.0));
    REQUIRE_THAT(p.render_instructions[1].sample_rate, WithinAbs(20e6, 1.0));
}

TEST_CASE("Duration sec preserved in render instructions", "[planner]") {
    auto result = plan(make_multi_device_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE_THAT(p.render_instructions[0].duration_sec, WithinAbs(5.0, 1e-12));
    REQUIRE_THAT(p.render_instructions[1].duration_sec, WithinAbs(3.0, 1e-12));
}

TEST_CASE("Planning rejects overlapping non-additive emitters on one channel", "[planner]") {
    Scenario s;
    s.metadata.name = "planner_overlap";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
    s.emitters.push_back({"cw1", "usrp0", 0, 0.0, 2.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.2}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});
    s.emitters.push_back({"cw2", "usrp0", 0, 1.0, 2.0,
                          WaveformDef{std::nullopt, WaveformType::CW, {{"amplitude", 0.3}}},
                          std::nullopt, std::nullopt, MixingMode::None, std::nullopt, std::nullopt});

    auto result = plan(s);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
    CHECK(result.error()[0].code == "V002_OVERLAPPING_EMITTERS");
}

TEST_CASE("Planner rejects unrenderable sample counts", "[planner]") {
    Scenario s = make_simple_scenario();
    s.devices[0].rf.rate_sps = static_cast<double>(std::numeric_limits<size_t>::max());
    s.emitters[0].duration_sec = 2.0;

    auto result = plan(s);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
    CHECK(result.error()[0].code == "E_PLAN_SAMPLE_COUNT_TOO_LARGE");
}

TEST_CASE("Start after sec values preserved in timeline", "[planner]") {
    auto result = plan(make_multi_device_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE_THAT(p.timeline[0].time_sec, WithinAbs(0.5, 1e-12));
    REQUIRE(p.timeline[0].target_id == "cw2");

    REQUIRE_THAT(p.timeline[1].time_sec, WithinAbs(1.0, 1e-12));
    REQUIRE(p.timeline[1].target_id == "cw1");
}

TEST_CASE("Empty scenario (no emitters) should fail planning", "[planner]") {
    Scenario s;
    s.metadata.name = "empty";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});

    auto result = plan(s);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
}

TEST_CASE("Planning fails for unknown device instead of returning partial plan", "[planner]") {
    auto s = make_simple_scenario();
    s.emitters[0].device = "missing";

    auto result = plan(s);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
    CHECK(result.error()[0].code == "E_PLAN_UNKNOWN_DEVICE");
}

TEST_CASE("Planning fails for unresolved waveform ref instead of returning partial plan", "[planner]") {
    auto s = make_simple_scenario();
    s.emitters[0].waveform = std::nullopt;
    s.emitters[0].waveform_ref = "missing_waveform";

    auto result = plan(s);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
    CHECK(result.error()[0].code == "E_PLAN_UNRESOLVED_WAVEFORM");
}

TEST_CASE("Planning rejects invalid timing and sample rates", "[planner]") {
    auto s = make_simple_scenario();
    s.emitters[0].start_after_sec = std::numeric_limits<double>::quiet_NaN();
    auto result = plan(s);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error()[0].code == "E_PLAN_INVALID_TIMING");

    s = make_simple_scenario();
    s.devices[0].rf.rate_sps = std::numeric_limits<double>::infinity();
    result = plan(s);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error()[0].code == "E_PLAN_INVALID_SAMPLE_RATE");
}

TEST_CASE("Planning rejects invalid repeat settings", "[planner]") {
    auto s = make_simple_scenario();
    s.emitters[0].repeat = RepeatSpec{0, 0.0};

    auto result = plan(s);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error()[0].code == "E_PLAN_INVALID_REPEAT");
}

TEST_CASE("Planning rejects malformed scenario events directly", "[planner][events]") {
    auto has_error = [](const ErrorList& errors, const std::string& code) {
        return std::any_of(errors.begin(), errors.end(), [&](const Error& error) {
            return error.code == code;
        });
    };

    SECTION("unknown type") {
        auto s = make_simple_scenario();
        s.events.push_back({"usrp0", 1.0, "unknown_type", nlohmann::json::object()});

        auto result = plan(s);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(has_error(result.error(), "E_PLAN_INVALID_EVENT_TYPE"));
    }

    SECTION("invalid timing") {
        auto s = make_simple_scenario();
        s.events.push_back({"usrp0", -1.0, "marker", nlohmann::json::object()});

        auto result = plan(s);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(has_error(result.error(), "E_PLAN_INVALID_EVENT_TIME"));
    }

    SECTION("unknown target device") {
        auto s = make_simple_scenario();
        s.events.push_back({"missing", 1.0, "retune", nlohmann::json{{"freq_hz", 2.4e9}}});

        auto result = plan(s);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(has_error(result.error(), "E_PLAN_EVENT_UNKNOWN_DEVICE"));
    }

    SECTION("missing retune payload") {
        auto s = make_simple_scenario();
        s.events.push_back({"usrp0", 1.0, "retune", nlohmann::json::object()});

        auto result = plan(s);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(has_error(result.error(), "E_PLAN_INVALID_EVENT_PAYLOAD"));
    }

    SECTION("invalid channel payload") {
        auto s = make_simple_scenario();
        s.events.push_back({"usrp0", 1.0, "gain_change",
                            nlohmann::json{{"gain_db", 12.0}, {"channel", -1}}});

        auto result = plan(s);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(has_error(result.error(), "E_PLAN_INVALID_EVENT_PAYLOAD"));
    }

    SECTION("unknown event channel") {
        auto s = make_simple_scenario();
        s.events.push_back({"usrp0", 1.0, "gain_change",
                            nlohmann::json{{"gain_db", 12.0}, {"channel", 7}}});

        auto result = plan(s);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(has_error(result.error(), "E_PLAN_EVENT_UNKNOWN_CHANNEL"));
    }

    SECTION("unknown waveform switch emitter") {
        auto s = make_simple_scenario();
        s.events.push_back({"usrp0", 1.0, "waveform_switch",
                            nlohmann::json{{"emitter_id", "missing"}, {"new_waveform", "wf2"}}});

        auto result = plan(s);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(has_error(result.error(), "E_PLAN_EVENT_UNKNOWN_EMITTER"));
    }

    SECTION("invalid impairment change payload") {
        auto s = make_simple_scenario();
        s.events.push_back({"usrp0", 1.0, "impairment_change",
                            nlohmann::json{{"emitter_id", "cw1"},
                                           {"impairment", "cfo_hz"},
                                           {"enabled", "yes"}}});

        auto result = plan(s);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(has_error(result.error(), "E_PLAN_INVALID_EVENT_PAYLOAD"));
    }
}

TEST_CASE("Warning emitted for emitter starting at time 0", "[planner]") {
    Scenario s;
    s.metadata.name = "zero_start";
    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});

    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 0.0;
    em.duration_sec = 5.0;
    em.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.5}}};
    s.emitters.push_back(em);

    auto result = plan(s);
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE_FALSE(p.warnings.empty());
    bool found = false;
    for (const auto& w : p.warnings) {
        if (w.code == "W_PLAN_ZERO_START") {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("No warning for emitter starting after time 0", "[planner]") {
    auto result = plan(make_simple_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    for (const auto& w : p.warnings) {
        REQUIRE(w.code != "W_PLAN_ZERO_START");
    }
}

TEST_CASE("Waveform ref resolved from named waveforms", "[planner]") {
    Scenario s;
    s.metadata.name = "waveform_ref_test";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});

    s.waveforms.push_back(WaveformDef{"my_chirp", WaveformType::Chirp, nlohmann::json{{"f0_hz", -2e6}, {"f1_hz", 2e6}}});

    EmitterDef em;
    em.id = "em1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 1.0;
    em.duration_sec = 5.0;
    em.waveform_ref = "my_chirp";
    s.emitters.push_back(em);

    auto result = plan(s);
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.render_instructions.size() == 1);
    REQUIRE(p.render_instructions[0].waveform.type == WaveformType::Chirp);
    REQUIRE(p.render_instructions[0].waveform.id.has_value());
    REQUIRE(*p.render_instructions[0].waveform.id == "my_chirp");
}

TEST_CASE("Normalized scenario preserves original data", "[planner]") {
    auto scenario = make_simple_scenario();
    auto result = plan(scenario);
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.normalized_scenario.metadata.name == "test_simple");
    REQUIRE(p.normalized_scenario.devices.size() == 1);
    REQUIRE(p.normalized_scenario.emitters.size() == 1);
}

TEST_CASE("EmitterStart event includes device_id and channel in payload", "[planner]") {
    auto result = plan(make_simple_scenario());
    REQUIRE(result.has_value());

    const auto& p = result.value();
    REQUIRE(p.timeline[0].payload.contains("device_id"));
    REQUIRE(p.timeline[0].payload["device_id"].get<std::string>() == "usrp0");
    REQUIRE(p.timeline[0].payload.contains("channel"));
    REQUIRE(p.timeline[0].payload["channel"].get<uint32_t>() == 0);
}
