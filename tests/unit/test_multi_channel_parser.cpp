#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/plan_io.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::ContainsSubstring;

static bool has_error_with_code(const ValidationResult& r, const std::string& code) {
    return std::any_of(r.errors.begin(), r.errors.end(),
                       [&](const Error& e) { return e.code == code; });
}

TEST_CASE("Multi-channel explicit mode parses correctly", "[multi_channel]") {
    const std::string json_str = R"({
        "metadata": { "name": "mc_test" },
        "devices": [
            { "id": "usrp0", "rf": { "freq_hz": 2450000000.0, "rate_sps": 10000000.0, "gain_db": 20.0 } }
        ],
        "channels": [
            { "id": "ch0", "device": "usrp0", "index": 0, "rf": { "freq_hz": 915000000.0, "rate_sps": 10000000.0, "gain_db": 20.0 } },
            { "id": "ch1", "device": "usrp0", "index": 1, "rf": { "freq_hz": 2450000000.0, "rate_sps": 10000000.0, "gain_db": 15.0 } }
        ],
        "sync_groups": [
            { "id": "pair", "channels": ["ch0", "ch1"], "mode": "coherent" }
        ],
        "emitters": [
            { "id": "cw0", "device": "usrp0", "channel_id": "ch0", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } },
            { "id": "cw1", "device": "usrp0", "channel_id": "ch1", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.3 } }
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& s = *result;

    REQUIRE(s.channel_defs.size() == 2);

    REQUIRE(s.channel_defs[0].id == "ch0");
    REQUIRE(s.channel_defs[0].device == "usrp0");
    REQUIRE(s.channel_defs[0].index == 0);
    REQUIRE_THAT(s.channel_defs[0].rf.freq_hz, WithinAbs(915e6, 1.0));
    REQUIRE_THAT(s.channel_defs[0].rf.rate_sps, WithinAbs(10e6, 1.0));
    REQUIRE_THAT(s.channel_defs[0].rf.gain_db, WithinAbs(20.0, 1e-12));

    REQUIRE(s.channel_defs[1].id == "ch1");
    REQUIRE(s.channel_defs[1].device == "usrp0");
    REQUIRE(s.channel_defs[1].index == 1);
    REQUIRE_THAT(s.channel_defs[1].rf.freq_hz, WithinAbs(2450e6, 1.0));
    REQUIRE_THAT(s.channel_defs[1].rf.gain_db, WithinAbs(15.0, 1e-12));

    REQUIRE(s.sync_groups.size() == 1);
    REQUIRE(s.sync_groups[0].id == "pair");
    REQUIRE(s.sync_groups[0].channels.size() == 2);
    REQUIRE(s.sync_groups[0].channels[0] == "ch0");
    REQUIRE(s.sync_groups[0].channels[1] == "ch1");
    REQUIRE(s.sync_groups[0].mode == "coherent");

    REQUIRE(s.emitters.size() == 2);
    REQUIRE(s.emitters[0].channel_id.has_value());
    REQUIRE(*s.emitters[0].channel_id == "ch0");
    REQUIRE(s.emitters[1].channel_id.has_value());
    REQUIRE(*s.emitters[1].channel_id == "ch1");
}

TEST_CASE("Multi-channel backward compatibility", "[multi_channel]") {
    const std::string json_str = R"({
        "metadata": { "name": "simple_test" },
        "devices": [
            { "id": "usrp0", "channel": 0, "rf": { "freq_hz": 1e9, "rate_sps": 5e6, "gain_db": 10.0 } }
        ],
        "emitters": [
            { "id": "cw1", "device": "usrp0", "channel": 0, "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& s = *result;

    REQUIRE(s.channel_defs.empty());
    REQUIRE(s.sync_groups.empty());
    REQUIRE_FALSE(s.emitters[0].channel_id.has_value());
}

TEST_CASE("Multi-channel emitter channel_id binding", "[multi_channel]") {
    const std::string json_str = R"({
        "metadata": { "name": "binding_test" },
        "devices": [
            { "id": "dev0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
        ],
        "channels": [
            { "id": "rx0", "device": "dev0", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } },
            { "id": "rx1", "device": "dev0", "index": 1, "rf": { "freq_hz": 2e9, "rate_sps": 10e6, "gain_db": 15.0 } }
        ],
        "emitters": [
            { "id": "e0", "device": "dev0", "channel_id": "rx0", "start_after_sec": 0.0, "duration_sec": 0.5, "waveform": { "type": "noise", "amplitude": 0.1 } },
            { "id": "e1", "device": "dev0", "channel_id": "rx1", "start_after_sec": 0.5, "duration_sec": 0.5, "waveform": { "type": "noise", "amplitude": 0.2 } }
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& s = *result;

    REQUIRE(s.emitters[0].channel_id.has_value());
    REQUIRE(*s.emitters[0].channel_id == "rx0");
    REQUIRE(s.emitters[1].channel_id.has_value());
    REQUIRE(*s.emitters[1].channel_id == "rx1");
}

TEST_CASE("Multi-channel validator rejects invalid channel refs", "[multi_channel]") {
    SECTION("Emitter references non-existent channel_id") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_ch_ref" },
            "devices": [
                { "id": "usrp0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "channels": [
                { "id": "ch0", "device": "usrp0", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "emitters": [
                { "id": "e0", "device": "usrp0", "channel_id": "nonexistent", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
            ]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE(result.has_value());
        auto vr = validate(*result);
        REQUIRE_FALSE(vr.ok());
        CHECK(has_error_with_code(vr, "V023_EMITTER_UNKNOWN_CHANNEL"));
    }

    SECTION("Duplicate channel IDs") {
        const std::string json_str = R"({
            "metadata": { "name": "dup_ch" },
            "devices": [
                { "id": "usrp0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "channels": [
                { "id": "ch0", "device": "usrp0", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } },
                { "id": "ch0", "device": "usrp0", "index": 1, "rf": { "freq_hz": 2e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "emitters": [
                { "id": "e0", "device": "usrp0", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
            ]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE(result.has_value());
        auto vr = validate(*result);
        REQUIRE_FALSE(vr.ok());
        CHECK(has_error_with_code(vr, "V020_DUPLICATE_CHANNEL_ID"));
    }

    SECTION("Channel references non-existent device") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_dev_ref" },
            "devices": [
                { "id": "usrp0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "channels": [
                { "id": "ch0", "device": "nonexistent", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "emitters": [
                { "id": "e0", "device": "usrp0", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
            ]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE(result.has_value());
        auto vr = validate(*result);
        REQUIRE_FALSE(vr.ok());
        CHECK(has_error_with_code(vr, "V021_CHANNEL_UNKNOWN_DEVICE"));
    }
}

TEST_CASE("Sync group validation", "[multi_channel]") {
    SECTION("Valid coherent sync group") {
        const std::string json_str = R"({
            "metadata": { "name": "valid_sync" },
            "devices": [
                { "id": "usrp0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "channels": [
                { "id": "ch0", "device": "usrp0", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } },
                { "id": "ch1", "device": "usrp0", "index": 1, "rf": { "freq_hz": 2e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "sync_groups": [
                { "id": "pair", "channels": ["ch0", "ch1"], "mode": "coherent" }
            ],
            "emitters": [
                { "id": "e0", "device": "usrp0", "channel_id": "ch0", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
            ]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE(result.has_value());
        auto vr = validate(*result);
        CHECK(vr.ok());
    }

    SECTION("Coherent with different devices") {
        const std::string json_str = R"({
            "metadata": { "name": "diff_dev" },
            "devices": [
                { "id": "usrp0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } },
                { "id": "usrp1", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "channels": [
                { "id": "ch0", "device": "usrp0", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } },
                { "id": "ch1", "device": "usrp1", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "sync_groups": [
                { "id": "pair", "channels": ["ch0", "ch1"], "mode": "coherent" }
            ],
            "emitters": [
                { "id": "e0", "device": "usrp0", "channel_id": "ch0", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
            ]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE(result.has_value());
        auto vr = validate(*result);
        REQUIRE_FALSE(vr.ok());
        CHECK(has_error_with_code(vr, "V025_COHERENT_DIFFERENT_DEVICES"));
    }

    SECTION("Coherent with different sample rates") {
        const std::string json_str = R"({
            "metadata": { "name": "diff_rate" },
            "devices": [
                { "id": "usrp0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "channels": [
                { "id": "ch0", "device": "usrp0", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } },
                { "id": "ch1", "device": "usrp0", "index": 1, "rf": { "freq_hz": 2e9, "rate_sps": 20e6, "gain_db": 20.0 } }
            ],
            "sync_groups": [
                { "id": "pair", "channels": ["ch0", "ch1"], "mode": "coherent" }
            ],
            "emitters": [
                { "id": "e0", "device": "usrp0", "channel_id": "ch0", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
            ]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE(result.has_value());
        auto vr = validate(*result);
        REQUIRE_FALSE(vr.ok());
        CHECK(has_error_with_code(vr, "V026_COHERENT_DIFFERENT_RATES"));
    }

    SECTION("Non-existent channel in sync group") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_sync_ch" },
            "devices": [
                { "id": "usrp0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "channels": [
                { "id": "ch0", "device": "usrp0", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
            ],
            "sync_groups": [
                { "id": "pair", "channels": ["ch0", "ghost"], "mode": "coherent" }
            ],
            "emitters": [
                { "id": "e0", "device": "usrp0", "channel_id": "ch0", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
            ]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE(result.has_value());
        auto vr = validate(*result);
        REQUIRE_FALSE(vr.ok());
        CHECK(has_error_with_code(vr, "V024_SYNC_UNKNOWN_CHANNEL"));
    }

    SECTION("Independent mode allows different devices") {
        const std::string json_str = R"({
            "metadata": { "name": "indep" },
            "devices": [
                { "id": "usrp0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } },
                { "id": "usrp1", "rf": { "freq_hz": 1e9, "rate_sps": 20e6, "gain_db": 20.0 } }
            ],
            "channels": [
                { "id": "ch0", "device": "usrp0", "index": 0, "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } },
                { "id": "ch1", "device": "usrp1", "index": 0, "rf": { "freq_hz": 2e9, "rate_sps": 20e6, "gain_db": 20.0 } }
            ],
            "sync_groups": [
                { "id": "pair", "channels": ["ch0", "ch1"], "mode": "independent" }
            ],
            "emitters": [
                { "id": "e0", "device": "usrp0", "channel_id": "ch0", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
            ]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE(result.has_value());
        auto vr = validate(*result);
        CHECK(vr.ok());
    }
}

TEST_CASE("Multi-channel plan_io round-trip", "[multi_channel]") {
    const std::string json_str = R"({
        "metadata": { "name": "roundtrip" },
        "devices": [
            { "id": "usrp0", "rf": { "freq_hz": 1e9, "rate_sps": 10e6, "gain_db": 20.0 } }
        ],
        "channels": [
            { "id": "ch0", "device": "usrp0", "index": 0, "rf": { "freq_hz": 915e6, "rate_sps": 10e6, "gain_db": 20.0 } },
            { "id": "ch1", "device": "usrp0", "index": 1, "rf": { "freq_hz": 2450e6, "rate_sps": 10e6, "gain_db": 15.0 } }
        ],
        "sync_groups": [
            { "id": "pair", "channels": ["ch0", "ch1"], "mode": "coherent" }
        ],
        "emitters": [
            { "id": "e0", "device": "usrp0", "channel_id": "ch0", "start_after_sec": 0.0, "duration_sec": 1.0, "waveform": { "type": "cw", "amplitude": 0.2 } }
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& original = *result;

    auto serialized = scenario_to_json(original);
    auto deserialized = scenario_from_json(serialized);
    REQUIRE(deserialized.has_value());
    const auto& s = *deserialized;

    REQUIRE(s.channel_defs.size() == 2);
    REQUIRE(s.channel_defs[0].id == "ch0");
    REQUIRE(s.channel_defs[1].id == "ch1");
    REQUIRE(s.sync_groups.size() == 1);
    REQUIRE(s.sync_groups[0].id == "pair");
    REQUIRE(s.sync_groups[0].mode == "coherent");
    REQUIRE(s.emitters[0].channel_id.has_value());
    REQUIRE(*s.emitters[0].channel_id == "ch0");
}
