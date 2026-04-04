add_library(archerfish_common STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rf_types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/regulatory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/safety_profile.cpp
)

target_include_directories(archerfish_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_common
    PUBLIC
        fmt::fmt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Warnings are applied per-target via cmake/CompilerWarnings.cmake

# Apply project warnings (requires cmake/CompilerWarnings.cmake included by parent)
if(COMMAND enable_project_warnings)
    enable_project_warnings(archerfish_common)
endif()
