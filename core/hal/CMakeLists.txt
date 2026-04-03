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
    target_link_libraries(archerfish_hal PUBLIC ${UHD_LIBRARIES})
    target_include_directories(archerfish_hal PUBLIC ${UHD_INCLUDE_DIRS})
    target_compile_definitions(archerfish_hal PUBLIC ARCHERFISH_HAS_UHD)
endif()
