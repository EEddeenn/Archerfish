add_library(archerfish_reporting STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/metrics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/run_directory.cpp
)

target_include_directories(archerfish_reporting
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_reporting
    PUBLIC
        archerfish_common
        archerfish_scheduler
)
