#include "archerfish/cli/cmd_schema.hpp"

#include <fstream>
#include <sstream>
#include <vector>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#ifndef CMAKE_SOURCE_DIR
#define CMAKE_SOURCE_DIR ""
#endif

namespace archerfish::cli {

namespace {

std::string read_schema_json() {
    std::string schema_path;
    // Try install location first
    #ifdef ARCHERFISH_INSTALL_DATADIR
    {
        std::string installed = std::string(ARCHERFISH_INSTALL_DATADIR) + "/schemas/scenario.schema.json";
        if (std::ifstream(installed).is_open()) {
            schema_path = installed;
        }
    }
    #endif
    // Fall back to source tree
    if (schema_path.empty()) {
        schema_path = std::string(CMAKE_SOURCE_DIR) + "/schemas/scenario.schema.json";
    }
    std::ifstream f(schema_path);
    if (!f.is_open()) {
        throw std::runtime_error(fmt::format("Cannot open schema file: {}", schema_path));
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string print_human(const nlohmann::json& schema) {
    std::ostringstream out;

    out << "Archerfish Scenario Schema\n";
    out << std::string(40, '=') << "\n\n";

    out << "Required fields:\n";
    if (schema.contains("required") && schema["required"].is_array()) {
        for (const auto& r : schema["required"]) {
            out << fmt::format("  - {}\n", r.get<std::string>());
        }
    }
    out << "\n";

    out << "Waveform types:\n";
    if (schema.contains("$defs") && schema["$defs"].contains("waveform") &&
        schema["$defs"]["waveform"].contains("properties") &&
        schema["$defs"]["waveform"]["properties"].contains("type") &&
        schema["$defs"]["waveform"]["properties"]["type"].contains("enum")) {
        for (const auto& t : schema["$defs"]["waveform"]["properties"]["type"]["enum"]) {
            out << fmt::format("  - {}\n", t.get<std::string>());
        }
    }
    out << "\n";

    out << "Impairments:\n";
    if (schema.contains("$defs") && schema["$defs"].contains("impairments") &&
        schema["$defs"]["impairments"].contains("properties")) {
        for (const auto& [key, val] : schema["$defs"]["impairments"]["properties"].items()) {
            std::string desc = val.value("description", "");
            out << fmt::format("  - {:<35s} {}\n", key, desc);
        }
    }

    return out.str();
}

std::string print_markdown(const nlohmann::json& schema) {
    std::ostringstream out;

    out << "| Waveform Type | Key Parameters |\n";
    out << "|---------------|----------------|\n";

    if (schema.contains("$defs") && schema["$defs"].contains("waveform") &&
        schema["$defs"]["waveform"]["properties"].contains("type") &&
        schema["$defs"]["waveform"]["properties"]["type"].contains("enum")) {
        for (const auto& t : schema["$defs"]["waveform"]["properties"]["type"]["enum"]) {
            std::string type = t.get<std::string>();
            std::string params;
            if (type == "cw") params = "`amplitude`";
            else if (type == "chirp") params = "`f0_hz`, `f1_hz`, `amplitude`";
            else if (type == "noise") params = "`amplitude`";
            else if (type == "bpsk" || type == "qpsk" || type == "8psk")
                params = "`symbol_rate`, `samples_per_symbol`, `rrc_alpha`, `amplitude`";
            else if (type == "qam16" || type == "qam64")
                params = "`symbol_rate`, `samples_per_symbol`, `rrc_alpha`, `amplitude`";
            else if (type == "multi_tone") params = "`tones` (array of `{frequency_hz, amplitude}`)";
            else if (type == "file") params = "`path`, `loop`";
            else if (type == "pulse") params = "`pulse_width_sec`, `pri_sec`, `amplitude`";
            else if (type == "ask") params = "`symbol_rate`, `amplitude`";
            else if (type == "fsk") params = "`symbol_rate`, `deviation_hz`, `amplitude`";
            else if (type == "am") params = "`mod_freq_hz`, `mod_depth`, `amplitude`";
            else if (type == "fm") params = "`mod_freq_hz`, `deviation_hz`, `amplitude`";
            else if (type == "pm") params = "`mod_freq_hz`, `mod_index`, `amplitude`";
            out << fmt::format("| `{}` | {} |\n", type, params);
        }
    }

    return out.str();
}

} // namespace

int cmd_schema_print(const CliOptions& opts, bool json, bool markdown) {
    try {
        std::string raw = read_schema_json();
        auto schema = nlohmann::json::parse(raw);
        (void)opts;

        if (json) {
            fmt::print("{}\n", schema.dump(2));
        } else if (markdown) {
            fmt::print("{}", print_markdown(schema));
        } else {
            fmt::print("{}", print_human(schema));
        }
        return 0;
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return static_cast<int>(ExitCode::GenericFailure);
    }
}

} // namespace archerfish::cli
