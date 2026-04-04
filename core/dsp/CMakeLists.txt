add_library(archerfish_dsp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/source_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cw_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chirp_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/noise_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/constellation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pulse_shaper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/multi_tone_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/file_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scaler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/summer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pulse_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ask_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fsk_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/am_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fm_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pm_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ofdm_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resampler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/waveform_metadata_io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/waveform_type.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/source_factory.cpp
)

target_include_directories(archerfish_dsp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(archerfish_dsp
    PUBLIC
        archerfish_common
    PRIVATE
        SampleRate::samplerate
)
