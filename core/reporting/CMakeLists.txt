add_library(archerfish_reporting STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/metrics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/run_directory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/emitter_metrics.cpp
)

target_include_directories(archerfish_reporting
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_reporting
    PUBLIC
        archerfish_common
        nlohmann_json::nlohmann_json
        archerfish_scheduler
    PRIVATE
        fmt::fmt
)
