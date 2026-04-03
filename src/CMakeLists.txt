add_executable(archerfish main.cpp)

# archerfish_cli brings all other archerfish libraries transitively
target_link_libraries(archerfish
    PRIVATE
        archerfish_cli
)

# Install is handled in the top-level CMakeLists.txt via GNUInstallDirs
