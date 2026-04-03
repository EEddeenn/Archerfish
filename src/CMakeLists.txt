add_executable(archerfish main.cpp)

target_link_libraries(archerfish
    PRIVATE
        archerfish_common
        archerfish_hal
        archerfish_runtime
        archerfish_dsp
        archerfish_impairments
        archerfish_scheduler
        archerfish_reporting
        archerfish_cli
        fmt::fmt
        spdlog::spdlog
        CLI11::CLI11
        nlohmann_json::nlohmann_json
)

# Install is handled in the top-level CMakeLists.txt via GNUInstallDirs
