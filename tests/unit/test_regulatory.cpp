#include <catch2/catch_test_macros.hpp>
#include <archerfish/common/regulatory.hpp>

#include <limits>

using namespace archerfish::common;

TEST_CASE("Default restricted bands are populated", "[regulatory]") {
    auto bands = get_default_restricted_bands();
    REQUIRE_FALSE(bands.empty());
    REQUIRE(bands.size() >= 4);
}

TEST_CASE("GPS L1 band triggers warning", "[regulatory]") {
    auto warnings = check_regulatory(1575.42e6);
    REQUIRE_FALSE(warnings.empty());
    bool found = false;
    for (const auto& w : warnings) {
        if (w.code == "W_REGULATED_BAND") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Aviation VHF band triggers warning", "[regulatory]") {
    auto warnings = check_regulatory(120e6);
    REQUIRE_FALSE(warnings.empty());
}

TEST_CASE("Safe frequency produces no warnings", "[regulatory]") {
    auto warnings = check_regulatory(2.4e9);
    REQUIRE(warnings.empty());
}

TEST_CASE("Bandwidth overlap detection", "[regulatory]") {
    auto warnings = check_regulatory(1.5e9, 200e6);
    REQUIRE_FALSE(warnings.empty());
}

TEST_CASE("Regulatory check rejects invalid numeric inputs", "[regulatory]") {
    auto nan_freq = check_regulatory(std::numeric_limits<double>::quiet_NaN());
    REQUIRE_FALSE(nan_freq.empty());
    REQUIRE(nan_freq[0].category == ErrorCategory::Validation);
    REQUIRE(nan_freq[0].code == "V_REGULATORY_INVALID_FREQUENCY");

    auto negative_freq = check_regulatory(-1.0);
    REQUIRE_FALSE(negative_freq.empty());
    REQUIRE(negative_freq[0].code == "V_REGULATORY_INVALID_FREQUENCY");

    auto bad_bandwidth = check_regulatory(2.4e9, std::numeric_limits<double>::infinity());
    REQUIRE_FALSE(bad_bandwidth.empty());
    REQUIRE(bad_bandwidth[0].code == "V_REGULATORY_INVALID_BANDWIDTH");

    auto negative_bandwidth = check_regulatory(2.4e9, -1.0);
    REQUIRE_FALSE(negative_bandwidth.empty());
    REQUIRE(negative_bandwidth[0].code == "V_REGULATORY_INVALID_BANDWIDTH");
}

TEST_CASE("Regulatory check rejects bandwidth that overflows frequency span", "[regulatory]") {
    auto warnings = check_regulatory(std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    REQUIRE_FALSE(warnings.empty());
    REQUIRE(warnings[0].category == ErrorCategory::Validation);
    REQUIRE(warnings[0].code == "V_REGULATORY_INVALID_BANDWIDTH");
}
