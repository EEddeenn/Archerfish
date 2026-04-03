#include "archerfish/cli/cmd_dryrun.hpp"
#include "archerfish/cli/pipeline.hpp"
#include "archerfish/cli/format.hpp"
#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include <fmt/format.h>

namespace archerfish::cli {

namespace {

std::string waveform_label(const scenario::WaveformDef& wf) {
    const auto& p = wf.params;
    if (wf.type == "cw") {
        double amp = p.value("amplitude", 0.0);
        return fmt::format("CW A={}", amp);
    }
    if (wf.type == "chirp") {
        double f0 = p.value("f0_hz", 0.0);
        double f1 = p.value("f1_hz", 0.0);
        double amp = p.value("amplitude", 0.0);
        return fmt::format("Chirp f0={}→{} A={}", format_freq(f0), format_freq(f1), amp);
    }
    if (wf.type == "pulse") {
        double freq = p.value("frequency_hz", 0.0);
        double pw = p.value("pulse_width_sec", 0.0);
        double pri = p.value("pri_sec", 0.0);
        return fmt::format("Pulse f={} PW={} PRI={}",
                           format_freq(freq), format_time(pw), format_time(pri));
    }
    if (wf.type == "noise") {
        double amp = p.value("amplitude", 0.0);
        return fmt::format("Noise A={}", amp);
    }
    if (wf.type == "qpsk" || wf.type == "bpsk" || wf.type == "8psk") {
        double sr = p.value("symbol_rate", 0.0);
        double amp = p.value("amplitude", 0.0);
        std::string type_upper = wf.type;
        std::transform(type_upper.begin(), type_upper.end(), type_upper.begin(), ::toupper);
        return fmt::format("{} {} A={}", type_upper, format_rate(sr), amp);
    }
    if (wf.type == "qam16" || wf.type == "qam64") {
        double sr = p.value("symbol_rate", 0.0);
        double amp = p.value("amplitude", 0.0);
        std::string type_upper = wf.type;
        std::transform(type_upper.begin(), type_upper.end(), type_upper.begin(), ::toupper);
        return fmt::format("{} {} A={}", type_upper, format_rate(sr), amp);
    }
    if (wf.type == "multi_tone") {
        return "MultiTone";
    }
    if (wf.type == "file") {
        return fmt::format("File({})", p.value("path", ""));
    }
    return wf.type;
}

struct TimelineEntry {
    double start;
    double stop;
    std::string emitter_id;
    std::string device_id;
    uint32_t channel;
    scenario::WaveformDef waveform;
};

std::string generate_timeline(const scenario::Plan& plan, const scenario::Scenario& scenario) {
    std::ostringstream out;

    const std::string hline(65, '\xE2');
    const std::string hline_thick(65, '\xE2');

    out << fmt::format("Scenario: \"{}\" | {} device{} | {} emitter{} | Duration: {:.1f}s\n",
                       scenario.metadata.name,
                       scenario.devices.size(),
                       scenario.devices.size() != 1 ? "s" : "",
                       scenario.emitters.size(),
                       scenario.emitters.size() != 1 ? "s" : "",
                       plan.estimated_duration_sec);
    out << std::string(65, '\xE2') << "\n";

    std::map<std::string, std::vector<TimelineEntry>> device_entries;
    std::map<std::string, const scenario::DeviceDef*> device_defs;

    for (const auto& dev : scenario.devices) {
        device_defs[dev.id] = &dev;
    }

    for (const auto& instr : plan.render_instructions) {
        TimelineEntry entry;
        entry.start = instr.start_sec;
        entry.stop = instr.start_sec + instr.duration_sec;
        entry.emitter_id = instr.emitter_id;
        entry.device_id = "unknown";
        entry.channel = 0;
        entry.waveform = instr.waveform;

        for (const auto& e : scenario.emitters) {
            if (e.id == instr.emitter_id) {
                entry.device_id = e.device;
                entry.channel = e.channel;
                break;
            }
        }

        device_entries[entry.device_id].push_back(entry);
    }

    for (auto& [dev_id, entries] : device_entries) {
        std::sort(entries.begin(), entries.end(),
                  [](const TimelineEntry& a, const TimelineEntry& b) {
                      return a.start < b.start;
                  });
    }

    for (const auto& [dev_id, entries] : device_entries) {
        auto dev_it = device_defs.find(dev_id);
        if (dev_it != device_defs.end()) {
            const auto& rf = dev_it->second->rf;
            uint32_t ch = dev_it->second->channel.value_or(0);
            out << fmt::format("Device: {} (ch{}) @ {}, {}, {} dB gain\n",
                               dev_id, ch,
                               format_freq(rf.freq_hz),
                               format_rate(rf.rate_sps),
                               rf.gain_db);
        } else {
            out << fmt::format("Device: {}\n", dev_id);
        }

        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            bool is_last = (i == entries.size() - 1);

            std::string branch_start = is_last ? "\xE2\x94\x94" : "\xE2\x94\x9C";
            std::string branch_cont = is_last ? " " : "\xE2\x94\x82";

            out << fmt::format("  {}{} {} [{:.3f}s \u2500\u2500\u2500 {:.3f}s] {}\n",
                               branch_start, "\xE2\x94\x80",
                               e.emitter_id, e.start, e.stop,
                               waveform_label(e.waveform));

            out << fmt::format("  {}    {}s: {} starts\n",
                               branch_cont, format_time(e.start), e.emitter_id);
            out << fmt::format("  {}    {}s: {} ends\n",
                               branch_cont, format_time(e.stop), e.emitter_id);
        }

        out << std::string(65, '\xE2') << "\n";
    }

    if (plan.estimated_duration_sec > 0) {
        out << std::string(65, '\xE2') << "\n";

        double total = plan.estimated_duration_sec;
        int num_ticks = 6;
        std::vector<double> tick_positions;
        for (int i = 0; i <= num_ticks; ++i) {
            tick_positions.push_back(total * i / num_ticks);
        }

        out << fmt::format("Timeline: {:.1f}s ", 0.0);
        for (int i = 0; i < num_ticks; ++i) {
            out << "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\xA4";
        }
        out << fmt::format(" {:.1f}s\n", total);

        out << "                ";
        for (int i = 1; i <= num_ticks; ++i) {
            out << fmt::format("{:.1f}  ", tick_positions[i]);
        }
        out << "\n";
    }

    return out.str();
}

} // namespace

int cmd_dryrun(const CliOptions& opts, const std::string& file_path) {
    auto result = cli::run_pipeline(file_path);
    if (!result.has_value()) {
        fmt::print(stderr, "Pipeline errors:\n");
        for (const auto& e : result.error()) {
            fmt::print(stderr, "  [{}] {} \xE2\x80\x94 {}\n",
                       common::category_to_string(e.category), e.code, e.message);
        }
        return static_cast<int>(ExitCode::InputValidationFailure);
    }

    if (opts.json_output) {
        nlohmann::json out;
        out["scenario"] = result->scenario.metadata.name;
        out["devices"] = result->scenario.devices.size();
        out["emitters"] = result->scenario.emitters.size();
        out["duration_sec"] = result->plan.estimated_duration_sec;

        auto instructions = nlohmann::json::array();
        for (const auto& instr : result->plan.render_instructions) {
            instructions.push_back({
                {"emitter_id", instr.emitter_id},
                {"start_sec", instr.start_sec},
                {"duration_sec", instr.duration_sec},
                {"waveform_type", instr.waveform.type},
            });
        }
        out["render_instructions"] = instructions;
        fmt::print("{}\n", out.dump(2));
    } else {
        auto timeline = generate_timeline(result->plan, result->scenario);
        fmt::print("{}", timeline);
    }

    return 0;
}

} // namespace archerfish::cli
