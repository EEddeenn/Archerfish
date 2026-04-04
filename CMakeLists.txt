cmake_minimum_required(VERSION 3.25)
project(archerfish VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")

# --- RPATH configuration ---
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

if(APPLE)
    set(CMAKE_MACOSX_RPATH TRUE)
    set(CMAKE_INSTALL_NAME_DIR "@rpath")
    set(CMAKE_INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN:${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif()

include(FetchContent)
include("${PROJECT_SOURCE_DIR}/cmake/CompilerWarnings.cmake")

enable_testing()
include(CTest)

# --- Sanitizer options (GCC/Clang only) ---
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
if(ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(ENABLE_UBSAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

# --- Dependencies (Homebrew + system) ---

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(CLI11 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Catch2 REQUIRED)
find_package(SampleRate REQUIRED)
find_package(Threads REQUIRED)

# json-schema-validator — not in Homebrew, fetch from source
FetchContent_Declare(
    json_schema_validator
    GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
    GIT_TAG        2.4.0
)
set(JSON_SCHEMA_VALIDATOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json_schema_validator)

# yaml-cpp — YAML scenario support (disabled: network fetch issue)
# FetchContent_Declare(
#     yaml_cpp
#     URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.8.0.tar.gz
# )
# set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(yaml_cpp)

# Optional: UHD (system-installed)
find_package(UHD QUIET)
if(UHD_FOUND)
    message(STATUS "UHD found — USRP support enabled")
else()
    message(STATUS "UHD not found — building without USRP support")
endif()

# --- Install schemas ---
install(DIRECTORY schemas/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/archerfish/schemas
    FILES_MATCHING PATTERN "*.json"
)

# Define the install datadir so installed binaries can find schemas
set(ARCHERFISH_INSTALL_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/archerfish")

# --- Project libraries ---
add_subdirectory(core)
add_subdirectory(cli)

# --- Executable ---
add_subdirectory(src)

# --- Tests ---
add_subdirectory(tests)

# --- Install the main executable ---
install(TARGETS archerfish
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install the FetchContent'd json-schema-validator so the export set can find it
install(TARGETS nlohmann_json_schema_validator
    EXPORT archerfish-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# --- Install all library targets with export set ---
install(
    TARGETS
        archerfish_common
        archerfish_hal
        archerfish_runtime
        archerfish_dsp
        archerfish_impairments
        archerfish_scheduler
        archerfish_reporting
        archerfish_cli
    EXPORT archerfish-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# --- Install headers for each module ---
install(DIRECTORY core/common/include/archerfish    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY core/hal/include/archerfish       DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY core/runtime/include/archerfish   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY core/dsp/include/archerfish       DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY core/impairments/include/archerfish DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY core/scheduler/include/archerfish DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY core/reporting/include/archerfish DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY cli/include/archerfish            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# --- Export CMake package config ---
install(EXPORT archerfish-targets
    FILE archerfish-targets.cmake
    NAMESPACE archerfish::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/archerfish
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/archerfish-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/archerfish-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/archerfish
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/archerfish-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/archerfish-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/archerfish-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/archerfish
)
