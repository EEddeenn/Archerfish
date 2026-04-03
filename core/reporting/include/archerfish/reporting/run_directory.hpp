#pragma once

#include <filesystem>
#include <string>

#include "archerfish/reporting/metrics.hpp"
#include "archerfish/reporting/report.hpp"
#include "archerfish/scenario/plan.hpp"
#include "archerfish/scenario/scenario.hpp"

namespace archerfish::reporting {

class RunDirectory {
public:
    RunDirectory(const std::filesystem::path& base_path,
                 const std::string& scenario_name);

    [[nodiscard]] const std::filesystem::path& path() const;

    void save_scenario(const scenario::Scenario& scenario);
    void save_plan(const scenario::Plan& plan);
    void save_metrics(const Metrics& metrics);
    void save_report(const Report& report);
    void append_log(const std::string& message);

    [[nodiscard]] std::filesystem::path scenario_path() const;
    [[nodiscard]] std::filesystem::path plan_path() const;
    [[nodiscard]] std::filesystem::path metrics_path() const;
    [[nodiscard]] std::filesystem::path report_path() const;
    [[nodiscard]] std::filesystem::path log_path() const;

private:
    std::filesystem::path dir_path_;
};

} // namespace archerfish::reporting
