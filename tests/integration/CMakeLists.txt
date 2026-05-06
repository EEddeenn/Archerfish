add_executable(test_runtime test_runtime.cpp)
target_link_libraries(test_runtime
    PRIVATE
        archerfish_runtime
        Catch2::Catch2WithMain
)
add_test(NAME integration_runtime COMMAND test_runtime)

# --- E2E Integration Tests (Increment 12) ---

function(add_e2e_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_compile_definitions(${test_name} PRIVATE EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
    target_link_libraries(${test_name}
        PRIVATE
            # archerfish_runtime brings: common, hal, dsp, scheduler, impairments
            archerfish_runtime
            archerfish_reporting
            Catch2::Catch2WithMain
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_e2e_test(test_e2e_cw test_e2e_cw.cpp)
add_e2e_test(test_e2e_chirp test_e2e_chirp.cpp)
add_e2e_test(test_e2e_qpsk test_e2e_qpsk.cpp)
add_e2e_test(test_e2e_replay test_e2e_replay.cpp)
add_e2e_test(test_e2e_pipeline test_e2e_pipeline.cpp)

add_executable(test_qpsk_impairments test_qpsk_impairments.cpp)
target_compile_definitions(test_qpsk_impairments PRIVATE EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
target_link_libraries(test_qpsk_impairments
    PRIVATE
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME integration_qpsk_impairments COMMAND test_qpsk_impairments)

# --- Multi-Channel Integration Tests (6.4) ---

add_e2e_test(test_multi_channel_run test_multi_channel_run.cpp)
