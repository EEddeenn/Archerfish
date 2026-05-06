#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/scenario.hpp"
#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"
#include "archerfish/runtime/event_dispatcher.hpp"
#include "archerfish/impairments/impairment_chain.hpp"
#include "archerfish/impairments/dc_offset.hpp"
#include "archerfish/impairments/phase_offset.hpp"

#include <atomic>
#include <chrono>
#include <complex>
#include <memory>
#include <vector>

using namespace archerfish::scenario;
using namespace archerfish::runtime;
using namespace archerfish::common;
using namespace archerfish::impairments;

static Scenario make_base_scenario() {
    Scenario s;
    s.metadata.name = "event_ext_test";

    DeviceDef dev;
    dev.id = "usrp0";
    dev.channel = 0;
    dev.rf = {2450000000.0, 10000000.0, 20.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 0.0;
    em.duration_sec = 2.0;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    s.emitters.push_back(em);

    return s;
}

static bool has_error_with_code(const ValidationResult& r, const std::string& code) {
    return std::any_of(r.errors.begin(), r.errors.end(),
                       [&](const Error& e) { return e.code == code; });
}

// ========================================================================
// Parser tests
// ========================================================================

TEST_CASE("Parse scenario with waveform_switch event", "[parser][event_extensions]") {
    std::string json_str = R"({
        "metadata": {"name": "wfs_test"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [
            {"id": "e1", "device": "d0", "duration_sec": 1.0,
             "waveform": {"type": "cw", "amplitude": 0.3}}
        ],
        "events": [
            {"target_device": "d0", "time_sec": 0.5, "type": "waveform_switch",
             "payload": {"emitter_id": "e1", "new_waveform": "chirp"}}
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    REQUIRE(result->events.size() == 1);
    REQUIRE(result->events[0].type == "waveform_switch");
    REQUIRE(result->events[0].payload["emitter_id"] == "e1");
    REQUIRE(result->events[0].payload["new_waveform"] == "chirp");
}

TEST_CASE("Parse scenario with impairment_change event", "[parser][event_extensions]") {
    std::string json_str = R"({
        "metadata": {"name": "imp_test"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [
            {"id": "e1", "device": "d0", "duration_sec": 1.0,
             "waveform": {"type": "cw", "amplitude": 0.3}}
        ],
        "events": [
            {"target_device": "d0", "time_sec": 0.3, "type": "impairment_change",
             "payload": {"emitter_id": "e1", "impairment": "cfo_hz", "enabled": false}}
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    REQUIRE(result->events.size() == 1);
    REQUIRE(result->events[0].type == "impairment_change");
    REQUIRE(result->events[0].payload["emitter_id"] == "e1");
    REQUIRE(result->events[0].payload["impairment"] == "cfo_hz");
    REQUIRE(result->events[0].payload["enabled"] == false);
}

// ========================================================================
// Validator tests
// ========================================================================

TEST_CASE("Valid waveform_switch event passes validation", "[validator][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.5;
    evt.type = "waveform_switch";
    evt.payload = nlohmann::json{{"emitter_id", "cw1"}, {"new_waveform", "chirp"}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE(result.ok());
}

TEST_CASE("Valid impairment_change event passes validation", "[validator][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.3;
    evt.type = "impairment_change";
    evt.payload = nlohmann::json{{"emitter_id", "cw1"}, {"impairment", "cfo_hz"}, {"enabled", false}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE(result.ok());
}

TEST_CASE("waveform_switch missing emitter_id produces error", "[validator][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.5;
    evt.type = "waveform_switch";
    evt.payload = nlohmann::json{{"new_waveform", "chirp"}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V019_WAVEFORM_SWITCH_MISSING_EMITTER"));
}

TEST_CASE("waveform_switch missing new_waveform produces error", "[validator][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.5;
    evt.type = "waveform_switch";
    evt.payload = nlohmann::json{{"emitter_id", "cw1"}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V019_WAVEFORM_SWITCH_MISSING_WAVEFORM"));
}

TEST_CASE("waveform_switch rejects non-string payload fields", "[validator][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.5;
    evt.type = "waveform_switch";
    evt.payload = nlohmann::json{{"emitter_id", 42}, {"new_waveform", false}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V019_WAVEFORM_SWITCH_INVALID_EMITTER"));
    CHECK(has_error_with_code(result, "V019_WAVEFORM_SWITCH_INVALID_WAVEFORM"));
}

TEST_CASE("impairment_change missing emitter_id produces error", "[validator][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.3;
    evt.type = "impairment_change";
    evt.payload = nlohmann::json{{"impairment", "cfo_hz"}, {"enabled", false}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V020_IMPAIRMENT_CHANGE_MISSING_EMITTER"));
}

TEST_CASE("impairment_change missing impairment name produces error", "[validator][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.3;
    evt.type = "impairment_change";
    evt.payload = nlohmann::json{{"emitter_id", "cw1"}, {"enabled", true}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V020_IMPAIRMENT_CHANGE_MISSING_IMPAIRMENT"));
}

TEST_CASE("impairment_change rejects invalid payload field types", "[validator][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.3;
    evt.type = "impairment_change";
    evt.payload = nlohmann::json{{"emitter_id", 42}, {"impairment", false}, {"enabled", "yes"}};
    s.events.push_back(evt);

    auto result = validate(s);
    REQUIRE_FALSE(result.ok());
    CHECK(has_error_with_code(result, "V020_IMPAIRMENT_CHANGE_INVALID_EMITTER"));
    CHECK(has_error_with_code(result, "V020_IMPAIRMENT_CHANGE_INVALID_IMPAIRMENT"));
    CHECK(has_error_with_code(result, "V020_IMPAIRMENT_CHANGE_INVALID_ENABLED"));
}

// ========================================================================
// Planner tests — new event types become timeline entries
// ========================================================================

TEST_CASE("Planner converts waveform_switch event to timeline", "[planner][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.5;
    evt.type = "waveform_switch";
    evt.payload = nlohmann::json{{"emitter_id", "cw1"}, {"new_waveform", "chirp"}};
    s.events.push_back(evt);

    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());

    bool found = false;
    for (const auto& te : plan_result->timeline) {
        if (te.type == TimelineEventType::WaveformSwitch) {
            found = true;
            REQUIRE(te.payload["emitter_id"] == "cw1");
            REQUIRE(te.payload["new_waveform"] == "chirp");
            REQUIRE(te.time_sec == 0.5);
        }
    }
    REQUIRE(found);
}

TEST_CASE("Planner converts impairment_change event to timeline", "[planner][event_extensions]") {
    auto s = make_base_scenario();
    ScenarioEvent evt;
    evt.target_device = "usrp0";
    evt.time_sec = 0.3;
    evt.type = "impairment_change";
    evt.payload = nlohmann::json{{"emitter_id", "cw1"}, {"impairment", "cfo_hz"}, {"enabled", false}};
    s.events.push_back(evt);

    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());

    bool found = false;
    for (const auto& te : plan_result->timeline) {
        if (te.type == TimelineEventType::ImpairmentChange) {
            found = true;
            REQUIRE(te.payload["emitter_id"] == "cw1");
            REQUIRE(te.payload["impairment"] == "cfo_hz");
            REQUIRE(te.payload["enabled"] == false);
            REQUIRE(te.time_sec == 0.3);
        }
    }
    REQUIRE(found);
}

// ========================================================================
// EventDispatcher tests — new dispatch record types
// ========================================================================

TEST_CASE("EventDispatcher stores WaveformSwitchDispatch records", "[runtime][event_extensions]") {
    EventDispatcher dispatcher;
    std::atomic<bool> fired{false};

    dispatcher.schedule(0.01, [&] {
        fired.store(true);
    });

    dispatcher.start();
    dispatcher.wait_complete();

    REQUIRE(fired.load());
    REQUIRE(dispatcher.dispatched_count() == 1);
    REQUIRE(dispatcher.waveform_switch_dispatches().empty());
    REQUIRE(dispatcher.impairment_change_dispatches().empty());
}

TEST_CASE("EventDispatcher accessor returns empty vectors initially", "[runtime][event_extensions]") {
    EventDispatcher dispatcher;
    CHECK(dispatcher.waveform_switch_dispatches().empty());
    CHECK(dispatcher.impairment_change_dispatches().empty());
}

// ========================================================================
// Impairment chain enable/disable integration
// ========================================================================

TEST_CASE("Impairment chain set_enabled toggles individual impairments", "[impairments][event_extensions]") {
    ImpairmentChain chain;
    chain.add(std::make_unique<DcOffsetImpairment>(1.0, 0.0));
    chain.add(std::make_unique<PhaseOffsetImpairment>(0.5));

    REQUIRE(chain.size() == 2);
    REQUIRE(chain.at(0).enabled());
    REQUIRE(chain.at(1).enabled());

    chain.set_enabled(0, false);
    REQUIRE_FALSE(chain.at(0).enabled());
    REQUIRE(chain.at(1).enabled());

    std::vector<std::complex<float>> data = {{1.0f, 0.0f}};
    chain.apply(data.data(), data.size());
    // PhaseOffsetImpairment(0.5) rotates by 0.5 rad: cos(0.5) + j*sin(0.5)
    REQUIRE(data[0].real() != 1.0f);
    REQUIRE(data[0].imag() != 0.0f);

    chain.set_enabled(0, true);
    REQUIRE(chain.at(0).enabled());
}

TEST_CASE("Impairment chain lookup by name for toggle", "[impairments][event_extensions]") {
    ImpairmentChain chain;
    chain.add(std::make_unique<DcOffsetImpairment>(0.5, 0.3));
    chain.add(std::make_unique<PhaseOffsetImpairment>(1.0));

    REQUIRE(chain.at(0).name() == "dc_offset");
    REQUIRE(chain.at(1).name() == "phase_offset");

    std::string target = "phase_offset";
    for (size_t i = 0; i < chain.size(); ++i) {
        if (chain.at(i).name() == target) {
            chain.set_enabled(i, false);
            break;
        }
    }

    REQUIRE(chain.at(0).enabled());
    REQUIRE_FALSE(chain.at(1).enabled());
}
