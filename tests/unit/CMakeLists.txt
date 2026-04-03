add_executable(archerfish_unit_tests test_main.cpp)

target_link_libraries(archerfish_unit_tests
    PRIVATE
        archerfish_common
        Catch2::Catch2WithMain
)

include(CTest)
add_test(NAME unit_build_sanity COMMAND archerfish_unit_tests)

add_executable(test_hal_stub test_hal_stub.cpp)

target_link_libraries(test_hal_stub
    PRIVATE
        archerfish_hal
        Catch2::Catch2WithMain
)

add_test(NAME unit_hal_stub COMMAND test_hal_stub)

# --- Increment 2: Common types tests ---

add_executable(test_error test_error.cpp)
target_link_libraries(test_error
    PRIVATE
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_error COMMAND test_error)

add_executable(test_rf_types test_rf_types.cpp)
target_link_libraries(test_rf_types
    PRIVATE
        archerfish_common
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_rf_types COMMAND test_rf_types)

add_executable(test_sample test_sample.cpp)
target_link_libraries(test_sample
    PRIVATE
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_sample COMMAND test_sample)

# --- Increment 5: Impairments tests ---

add_executable(test_impairments test_impairments.cpp)
target_link_libraries(test_impairments
    PRIVATE
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME unit_impairments COMMAND test_impairments)

add_executable(test_impairment_chain test_impairment_chain.cpp)
target_link_libraries(test_impairment_chain
    PRIVATE
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME unit_impairment_chain COMMAND test_impairment_chain)

# --- Increment 4: DSP waveform generator tests ---

add_executable(test_cw_source test_cw_source.cpp)
target_link_libraries(test_cw_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_cw_source COMMAND test_cw_source)

add_executable(test_chirp_source test_chirp_source.cpp)
target_link_libraries(test_chirp_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_chirp_source COMMAND test_chirp_source)

add_executable(test_noise_source test_noise_source.cpp)
target_link_libraries(test_noise_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_noise_source COMMAND test_noise_source)

add_executable(test_modulator test_modulator.cpp)
target_link_libraries(test_modulator
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_modulator COMMAND test_modulator)

add_executable(test_multi_tone test_multi_tone.cpp)
target_link_libraries(test_multi_tone
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_multi_tone COMMAND test_multi_tone)

add_executable(test_file_source test_file_source.cpp)
target_compile_definitions(test_file_source PRIVATE TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../fixtures")
target_link_libraries(test_file_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_file_source COMMAND test_file_source)

add_executable(test_processing_blocks test_processing_blocks.cpp)
target_link_libraries(test_processing_blocks
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_processing_blocks COMMAND test_processing_blocks)

# --- Increment 6: Scenario data model + parser tests ---

add_executable(test_scenario_model test_scenario_model.cpp)
target_link_libraries(test_scenario_model
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_scenario_model COMMAND test_scenario_model)

add_executable(test_parser test_parser.cpp)
target_compile_definitions(test_parser PRIVATE EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
target_link_libraries(test_parser
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_parser COMMAND test_parser)

# --- Increment 7: Scenario validator tests ---

add_executable(test_validator test_validator.cpp)
target_link_libraries(test_validator
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_validator COMMAND test_validator)

# --- Increment 8: Planner tests ---

add_executable(test_planner test_planner.cpp)
target_link_libraries(test_planner
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_planner COMMAND test_planner)

add_executable(test_plan_io test_plan_io.cpp)
target_link_libraries(test_plan_io
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_plan_io COMMAND test_plan_io)

# --- Increment 9: Runtime engine tests ---

add_executable(test_runtime_state test_runtime_state.cpp)
target_link_libraries(test_runtime_state
    PRIVATE
        archerfish_runtime
        Catch2::Catch2WithMain
)
add_test(NAME unit_runtime_state COMMAND test_runtime_state)

add_executable(test_spsc_queue test_spsc_queue.cpp)
target_link_libraries(test_spsc_queue
    PRIVATE
        archerfish_runtime
        Catch2::Catch2WithMain
)
add_test(NAME unit_spsc_queue COMMAND test_spsc_queue)

add_executable(test_render_worker test_render_worker.cpp)
target_link_libraries(test_render_worker
    PRIVATE
        archerfish_runtime
        Catch2::Catch2WithMain
)
add_test(NAME unit_render_worker COMMAND test_render_worker)

add_executable(test_tx_worker test_tx_worker.cpp)
target_link_libraries(test_tx_worker
    PRIVATE
        archerfish_runtime
        Catch2::Catch2WithMain
)
add_test(NAME unit_tx_worker COMMAND test_tx_worker)

add_executable(test_event_dispatcher test_event_dispatcher.cpp)
target_link_libraries(test_event_dispatcher
    PRIVATE
        archerfish_runtime
        Catch2::Catch2WithMain
)
add_test(NAME unit_event_dispatcher COMMAND test_event_dispatcher)

# --- Increment 10: CLI tests ---

add_executable(test_cli_app test_cli_app.cpp)
target_link_libraries(test_cli_app
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME cli_app COMMAND test_cli_app)

add_executable(test_cmd_devices test_cmd_devices.cpp)
target_link_libraries(test_cmd_devices
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME cli_cmd_devices COMMAND test_cmd_devices)

add_executable(test_cmd_scenario test_cmd_scenario.cpp)
target_compile_definitions(test_cmd_scenario PRIVATE EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
target_link_libraries(test_cmd_scenario
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME cli_cmd_scenario COMMAND test_cmd_scenario)

add_executable(test_cmd_wave test_cmd_wave.cpp)
target_link_libraries(test_cmd_wave
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME cli_cmd_wave COMMAND test_cmd_wave)

# --- Increment 11: Reporting tests ---

add_executable(test_metrics test_metrics.cpp)
target_link_libraries(test_metrics
    PRIVATE
        archerfish_reporting
        Catch2::Catch2WithMain
)
add_test(NAME reporting_metrics COMMAND test_metrics)

add_executable(test_report test_report.cpp)
target_link_libraries(test_report
    PRIVATE
        archerfish_reporting
        Catch2::Catch2WithMain
)
add_test(NAME reporting_report COMMAND test_report)

add_executable(test_run_directory test_run_directory.cpp)
target_link_libraries(test_run_directory
    PRIVATE
        archerfish_reporting
        Catch2::Catch2WithMain
)
add_test(NAME reporting_run_directory COMMAND test_run_directory)

# --- Phase 2: Schema Validation ---

add_executable(test_schema_validation test_schema_validation.cpp)
target_compile_definitions(test_schema_validation PRIVATE EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
target_link_libraries(test_schema_validation
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_schema_validation COMMAND test_schema_validation)

# --- Phase 2: Resampler ---

add_executable(test_resampler test_resampler.cpp)
target_link_libraries(test_resampler
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME unit_resampler COMMAND test_resampler)

# --- Phase 2: New Waveform Sources ---

add_executable(test_pulse_source test_pulse_source.cpp)
target_link_libraries(test_pulse_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_pulse_source COMMAND test_pulse_source)

add_executable(test_ask_fsk_source test_ask_fsk_source.cpp)
target_link_libraries(test_ask_fsk_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_ask_fsk_source COMMAND test_ask_fsk_source)

add_executable(test_am_fm_pm_source test_am_fm_pm_source.cpp)
target_link_libraries(test_am_fm_pm_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_am_fm_pm_source COMMAND test_am_fm_pm_source)

# --- Phase 2: New Impairments ---

add_executable(test_new_impairments test_new_impairments.cpp)
target_link_libraries(test_new_impairments
    PRIVATE
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME unit_new_impairments COMMAND test_new_impairments)

# --- Phase 2: Sidecar Metadata ---

add_executable(test_waveform_metadata_io test_waveform_metadata_io.cpp)
target_link_libraries(test_waveform_metadata_io
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME unit_waveform_metadata_io COMMAND test_waveform_metadata_io)

# --- Phase 2: Reporting Improvements ---

add_executable(test_emitter_metrics test_emitter_metrics.cpp)
target_link_libraries(test_emitter_metrics
    PRIVATE
        archerfish_reporting
        Catch2::Catch2WithMain
)
add_test(NAME unit_emitter_metrics COMMAND test_emitter_metrics)

# --- Phase 2: Dry-Run ---

add_executable(test_dryrun test_dryrun.cpp)
target_link_libraries(test_dryrun
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_dryrun COMMAND test_dryrun)
