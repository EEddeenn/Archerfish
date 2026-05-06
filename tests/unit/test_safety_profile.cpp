#include <catch2/catch_test_macros.hpp>
#include <archerfish/common/safety_profile.hpp>

#include <limits>

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

TEST_CASE("Safety profile rejects invalid numeric inputs", "[safety_profile]") {
    auto profile = get_lab_safe_profile();

    auto infinite_gain = check_safety_profile(profile, std::numeric_limits<double>::infinity(), 0.3, 2.4e9);
    REQUIRE_FALSE(infinite_gain.empty());
    REQUIRE(infinite_gain[0].category == ErrorCategory::Validation);
    REQUIRE(infinite_gain[0].code == "V_SAFETY_NONFINITE_INPUT");

    auto nan_amplitude = check_safety_profile(profile, 10.0, std::numeric_limits<double>::quiet_NaN(), 2.4e9);
    REQUIRE_FALSE(nan_amplitude.empty());
    REQUIRE(nan_amplitude[0].code == "V_SAFETY_NONFINITE_INPUT");

    auto negative_amplitude = check_safety_profile(profile, 10.0, -0.1, 2.4e9);
    REQUIRE_FALSE(negative_amplitude.empty());
    REQUIRE(negative_amplitude[0].code == "V_SAFETY_INVALID_AMPLITUDE");

    auto invalid_frequency = check_safety_profile(profile, 10.0, 0.1, 0.0);
    REQUIRE_FALSE(invalid_frequency.empty());
    REQUIRE(invalid_frequency[0].code == "V_SAFETY_INVALID_FREQUENCY");
}

TEST_CASE("Safety profile rejects invalid profile limits", "[safety_profile]") {
    SafetyProfile bad = get_lab_safe_profile();
    bad.max_freq_hz = bad.min_freq_hz;

    auto errors = check_safety_profile(bad, 10.0, 0.1, 2.4e9);
    REQUIRE_FALSE(errors.empty());
    REQUIRE(errors[0].category == ErrorCategory::Validation);
    REQUIRE(errors[0].code == "V_SAFETY_INVALID_PROFILE");
}
