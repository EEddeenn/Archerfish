add_library(archerfish_cli STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_devices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_scenario.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_wave.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_doctor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_dryrun.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_schema.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd_calib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline.cpp
)

target_compile_definitions(archerfish_cli
    PRIVATE
        CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        ARCHERFISH_INSTALL_DATADIR="${ARCHERFISH_INSTALL_DATADIR}"
)

target_include_directories(archerfish_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_cli
    PUBLIC
        # archerfish_runtime brings: common, hal, dsp, scheduler, impairments
        # archerfish_reporting brings: common, scheduler
        archerfish_runtime
        archerfish_reporting
        CLI11::CLI11
)
