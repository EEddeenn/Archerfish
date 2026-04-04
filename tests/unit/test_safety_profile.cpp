#include <catch2/catch_test_macros.hpp>
#include <archerfish/common/safety_profile.hpp>

using namespace archerfish::common;

TEST_CASE("Lab safe profile has correct defaults", "[safety_profile]") {
    auto profile = get_lab_safe_profile();
    REQUIRE(profile.name == "lab_safe");
    REQUIRE(profile.max_gain_db == 20.0);
    REQUIRE(profile.max_amplitude == 0.5);
    REQUIRE(profile.min_freq_hz == 900e6);
    REQUIRE(profile.max_freq_hz == 6e9);
}

TEST_CASE("Safe parameters pass validation", "[safety_profile]") {
    auto profile = get_lab_safe_profile();
    auto errors = check_safety_profile(profile, 15.0, 0.3, 2.4e9);
    REQUIRE(errors.empty());
}

TEST_CASE("Excessive gain triggers error", "[safety_profile]") {
    auto profile = get_lab_safe_profile();
    auto errors = check_safety_profile(profile, 25.0, 0.3, 2.4e9);
    REQUIRE_FALSE(errors.empty());
    bool found = false;
    for (const auto& e : errors) {
        if (e.code == "W_SAFETY_GAIN_EXCEEDED") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Excessive amplitude triggers error", "[safety_profile]") {
    auto profile = get_lab_safe_profile();
    auto errors = check_safety_profile(profile, 15.0, 0.8, 2.4e9);
    REQUIRE_FALSE(errors.empty());
    bool found = false;
    for (const auto& e : errors) {
        if (e.code == "W_SAFETY_AMPLITUDE_EXCEEDED") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Out-of-range frequency triggers error", "[safety_profile]") {
    auto profile = get_lab_safe_profile();
    auto errors = check_safety_profile(profile, 15.0, 0.3, 100e6);
    REQUIRE_FALSE(errors.empty());
    bool found = false;
    for (const auto& e : errors) {
        if (e.code == "W_SAFETY_FREQ_OUT_OF_RANGE") found = true;
    }
    REQUIRE(found);
}
