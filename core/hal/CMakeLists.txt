set(HAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stub_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hal_factory.cpp
)

if(UHD_FOUND)
    list(APPEND HAL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/uhd_device.cpp)
endif()

add_library(archerfish_hal STATIC ${HAL_SOURCES})

target_include_directories(archerfish_hal
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_hal
    PUBLIC
        archerfish_common
)

if(UHD_FOUND)
    # UHD's CMake config may not create an imported target — create one if missing
    if(NOT TARGET UHD::uhd)
        add_library(UHD::uhd UNKNOWN IMPORTED)
        set_target_properties(UHD::uhd PROPERTIES
            IMPORTED_LOCATION "${UHD_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${UHD_INCLUDE_DIRS}"
        )
    endif()
    target_link_libraries(archerfish_hal PUBLIC UHD::uhd)
    target_compile_definitions(archerfish_hal PUBLIC ARCHERFISH_HAS_UHD)
endif()
