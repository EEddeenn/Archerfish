add_library(archerfish_scheduler STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/validator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/planner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plan_io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/schema_validator.cpp
)

target_include_directories(archerfish_scheduler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(archerfish_scheduler
    PRIVATE
        CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(archerfish_scheduler
    PUBLIC
        archerfish_common
    PRIVATE
        nlohmann_json::nlohmann_json
        nlohmann_json_schema_validator::validator
)
