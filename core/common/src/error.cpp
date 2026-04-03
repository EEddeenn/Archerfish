#include "archerfish/common/error.hpp"

#include <fmt/format.h>

namespace archerfish::common {

std::string category_to_string(ErrorCategory cat) {
    switch (cat) {
        case ErrorCategory::Config:          return "Config";
        case ErrorCategory::Validation:      return "Validation";
        case ErrorCategory::Planning:        return "Planning";
        case ErrorCategory::Preparation:     return "Preparation";
        case ErrorCategory::Execution:       return "Execution";
        case ErrorCategory::QualityWarning:  return "QualityWarning";
    }
    return "Unknown";
}

ErrorCategory category_from_string(const std::string& s) {
    if (s == "Config")          return ErrorCategory::Config;
    if (s == "Validation")      return ErrorCategory::Validation;
    if (s == "Planning")        return ErrorCategory::Planning;
    if (s == "Preparation")     return ErrorCategory::Preparation;
    if (s == "Execution")       return ErrorCategory::Execution;
    if (s == "QualityWarning")  return ErrorCategory::QualityWarning;
    return ErrorCategory::Config;
}

std::string error_to_string(const Error& err) {
    return fmt::format("{}: {} — {}", category_to_string(err.category), err.code, err.message);
}

} // namespace archerfish::common
