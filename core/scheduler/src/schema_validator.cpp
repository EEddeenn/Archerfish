#include "archerfish/scenario/schema_validator.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json-schema.hpp>

#include <fmt/format.h>

namespace archerfish::scenario {

using json = nlohmann::json;
using namespace archerfish::common;

namespace {

std::filesystem::path find_schema_file() {
    // Try install location first (works after cmake --install)
    #ifdef ARCHERFISH_INSTALL_DATADIR
    {
        auto installed = std::filesystem::path(ARCHERFISH_INSTALL_DATADIR) / "schemas" / "scenario.schema.json";
        if (std::filesystem::exists(installed)) {
            return installed;
        }
    }
    #endif
    // Fall back to source tree (works during development)
    auto src = std::filesystem::path(CMAKE_SOURCE_DIR);
    return src / "schemas" / "scenario.schema.json";
}

json load_schema_from_file() {
    auto schema_path = find_schema_file();
    std::ifstream ifs(schema_path);
    if (!ifs.is_open()) {
        throw std::runtime_error(
            fmt::format("Cannot open scenario schema: {}", schema_path.string()));
    }
    return json::parse(ifs);
}

class collecting_error_handler : public nlohmann::json_schema::basic_error_handler {
public:
    explicit collecting_error_handler(ErrorList& errors)
        : errors_(errors) {}

    void error(const nlohmann::json::json_pointer& ptr,
               const nlohmann::json& /*instance*/,
               const std::string& message) override {
        basic_error_handler::error(ptr, {}, message);
        errors_.push_back(Error{
            ErrorCategory::Config,
            "E_SCHEMA_VALIDATION",
            fmt::format("{}: {}", ptr.to_string(), message),
        });
    }

private:
    ErrorList& errors_;
};

} // namespace

nlohmann::json get_scenario_schema() {
    static json schema = load_schema_from_file();
    return schema;
}

std::expected<void, ErrorList> validate_schema(const json& instance) {
    ErrorList errors;

    try {
        auto schema = get_scenario_schema();
        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(schema);

        collecting_error_handler handler(errors);
        validator.validate(instance, handler);
    } catch (const std::exception& e) {
        errors.push_back(Error{
            ErrorCategory::Config,
            "E_SCHEMA_SETUP",
            fmt::format("Schema validation setup failed: {}", e.what()),
        });
    }

    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }
    return {};
}

} // namespace archerfish::scenario
