#include <catch2/catch_test_macros.hpp>

#include "archerfish/common/error.hpp"

using namespace archerfish::common;

TEST_CASE("ErrorCategory values are distinct", "[common][error]") {
    Error categories[] = {
        Error{ErrorCategory::Config,         "C1", "c1"},
        Error{ErrorCategory::Validation,     "C2", "c2"},
        Error{ErrorCategory::Planning,       "C3", "c3"},
        Error{ErrorCategory::Preparation,    "C4", "c4"},
        Error{ErrorCategory::Execution,      "C5", "c5"},
        Error{ErrorCategory::QualityWarning, "C6", "c6"},
    };
    REQUIRE(std::size(categories) == 6);
}

TEST_CASE("Error construction and field access", "[common][error]") {
    Error err{ErrorCategory::Validation, "E001_INVALID_AMPLITUDE", "Amplitude must be positive"};
    REQUIRE(err.category == ErrorCategory::Validation);
    REQUIRE(err.code == "E001_INVALID_AMPLITUDE");
    REQUIRE(err.message == "Amplitude must be positive");
}

TEST_CASE("category_to_string returns correct strings", "[common][error]") {
    CHECK(category_to_string(ErrorCategory::Config)         == "Config");
    CHECK(category_to_string(ErrorCategory::Validation)     == "Validation");
    CHECK(category_to_string(ErrorCategory::Planning)       == "Planning");
    CHECK(category_to_string(ErrorCategory::Preparation)    == "Preparation");
    CHECK(category_to_string(ErrorCategory::Execution)      == "Execution");
    CHECK(category_to_string(ErrorCategory::QualityWarning) == "QualityWarning");
}

TEST_CASE("error_to_string formats nicely", "[common][error]") {
    Error err{ErrorCategory::Validation, "E001_BAD_FREQ", "Frequency must be positive"};
    auto s = error_to_string(err);
    CHECK(s == "Validation: E001_BAD_FREQ — Frequency must be positive");
}

TEST_CASE("ErrorList works as a vector", "[common][error]") {
    ErrorList errors;
    errors.push_back({ErrorCategory::Config, "C1", "msg1"});
    errors.push_back({ErrorCategory::Execution, "C2", "msg2"});
    REQUIRE(errors.size() == 2);
    CHECK(errors[0].category == ErrorCategory::Config);
    CHECK(errors[1].category == ErrorCategory::Execution);
}
