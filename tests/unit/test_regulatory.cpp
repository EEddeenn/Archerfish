#include <catch2/catch_test_macros.hpp>
#include <archerfish/common/regulatory.hpp>

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
