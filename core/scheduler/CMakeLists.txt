add_library(archerfish_scheduler STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/validator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/planner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plan_io.cpp
)

target_include_directories(archerfish_scheduler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_scheduler
    PUBLIC
        archerfish_common
)
