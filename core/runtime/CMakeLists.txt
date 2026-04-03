add_library(archerfish_runtime STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tx_worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/event_dispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime.cpp
)

target_include_directories(archerfish_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_runtime
    PUBLIC
        archerfish_common
        archerfish_hal
        archerfish_dsp
        archerfish_scheduler
)
