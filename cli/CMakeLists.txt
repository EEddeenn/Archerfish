add_library(archerfish_cli STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_devices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_scenario.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_wave.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_doctor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_dryrun.cpp
)

target_include_directories(archerfish_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_cli
    PUBLIC
        archerfish_common
        archerfish_hal
        archerfish_scheduler
        archerfish_runtime
        archerfish_dsp
        archerfish_reporting
        CLI11::CLI11
        fmt::fmt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)
