add_library(archerfish_common STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rf_types.cpp
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
