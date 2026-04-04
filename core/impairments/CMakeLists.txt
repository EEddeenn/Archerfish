add_library(archerfish_impairments STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/awgn.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/phase_offset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/iq_imbalance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dc_offset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impairment_chain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/amplitude_ripple.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/delay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/burst_dropout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fading.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/multipath.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pa_nonlinearity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/phase_noise.cpp
)

target_include_directories(archerfish_impairments
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_impairments
    PUBLIC
        archerfish_common
        archerfish_scheduler
)
