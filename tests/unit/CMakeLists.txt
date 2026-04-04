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

add_executable(test_spsc_concurrent test_spsc_concurrent.cpp)
target_link_libraries(test_spsc_concurrent
    PRIVATE
        archerfish_runtime
        Catch2::Catch2WithMain
)
add_test(NAME unit_spsc_concurrent COMMAND test_spsc_concurrent)

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

add_executable(test_source_base test_source_base.cpp)
target_link_libraries(test_source_base
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_source_base COMMAND test_source_base)

add_executable(test_waveform_type test_waveform_type.cpp)
target_link_libraries(test_waveform_type
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME unit_waveform_type COMMAND test_waveform_type)

add_executable(test_apsk_constellation test_apsk_constellation.cpp)
target_link_libraries(test_apsk_constellation
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_apsk_constellation COMMAND test_apsk_constellation)

add_executable(test_ofdm_source test_ofdm_source.cpp)
target_link_libraries(test_ofdm_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_ofdm_source COMMAND test_ofdm_source)

# --- Phase 4: Schema & Calibration CLI commands ---

add_executable(test_cmd_schema test_cmd_schema.cpp)
target_compile_definitions(test_cmd_schema PRIVATE CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(test_cmd_schema
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME cli_cmd_schema COMMAND test_cmd_schema)

add_executable(test_calibration test_calibration.cpp)
target_link_libraries(test_calibration
    PRIVATE
        archerfish_reporting
        Catch2::Catch2WithMain
)
add_test(NAME reporting_calibration COMMAND test_calibration)

add_executable(test_cmd_calib test_cmd_calib.cpp)
target_link_libraries(test_cmd_calib
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME cli_cmd_calib COMMAND test_cmd_calib)

# --- Phase 4.3: Multi-Emitter Additive Mixing ---

add_executable(test_additive_mixer test_additive_mixer.cpp)
target_link_libraries(test_additive_mixer
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_additive_mixer COMMAND test_additive_mixer)

# --- Phase 5.1: Timed Retune and Gain Changes ---

add_executable(test_timed_events test_timed_events.cpp)
target_link_libraries(test_timed_events
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_timed_events COMMAND test_timed_events)

# --- Phase 5.2: Repeated Burst Scheduling ---

add_executable(test_burst_repeat test_burst_repeat.cpp)
target_link_libraries(test_burst_repeat
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_burst_repeat COMMAND test_burst_repeat)

# --- Phase 5.3: Marker Events ---

add_executable(test_marker_events test_marker_events.cpp)
target_link_libraries(test_marker_events
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_marker_events COMMAND test_marker_events)

# --- Phase 5.4: Planning Diagnostics ---

add_executable(test_planning_diagnostics test_planning_diagnostics.cpp)
target_link_libraries(test_planning_diagnostics
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_planning_diagnostics COMMAND test_planning_diagnostics)

# --- Increment 6.1: Multi-Channel Parser & Validator ---

add_executable(test_multi_channel_parser test_multi_channel_parser.cpp)
target_link_libraries(test_multi_channel_parser
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_multi_channel_parser COMMAND test_multi_channel_parser)

add_executable(test_multi_channel_planner test_multi_channel_planner.cpp)
target_link_libraries(test_multi_channel_planner
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_multi_channel_planner COMMAND test_multi_channel_planner)

# --- Phase 7.2.1: Phase Noise Impairment ---

add_executable(test_phase_noise test_phase_noise.cpp)
target_link_libraries(test_phase_noise
    PRIVATE
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME unit_phase_noise COMMAND test_phase_noise)

# --- Phase 7.2.2: Multipath Impairment ---

add_executable(test_multipath test_multipath.cpp)
target_link_libraries(test_multipath
    PRIVATE
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME unit_multipath COMMAND test_multipath)

# --- Phase 7.2.3: Fading Impairment ---

add_executable(test_fading test_fading.cpp)
target_link_libraries(test_fading
    PRIVATE
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME unit_fading COMMAND test_fading)

# --- Phase 7.2.4: PA Nonlinearity Impairment ---

add_executable(test_pa_nonlinearity test_pa_nonlinearity.cpp)
target_link_libraries(test_pa_nonlinearity
    PRIVATE
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME unit_pa_nonlinearity COMMAND test_pa_nonlinearity)

# --- Phase 7.2.5: Scheduled Impairment State Changes ---

add_executable(test_scheduled_impairment test_scheduled_impairment.cpp)
target_link_libraries(test_scheduled_impairment
    PRIVATE
        archerfish_scheduler
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME unit_scheduled_impairment COMMAND test_scheduled_impairment)

# --- Event Extensions: waveform_switch & impairment_change events ---

add_executable(test_event_extensions test_event_extensions.cpp)
target_link_libraries(test_event_extensions
    PRIVATE
        archerfish_scheduler
        archerfish_runtime
        archerfish_impairments
        Catch2::Catch2WithMain
)
add_test(NAME unit_event_extensions COMMAND test_event_extensions)

# --- Multi-Channel HAL Tests (6.3) ---

add_executable(test_multi_channel_hal test_multi_channel_hal.cpp)
target_link_libraries(test_multi_channel_hal
    PRIVATE
        archerfish_hal
        Catch2::Catch2WithMain
)
add_test(NAME unit_multi_channel_hal COMMAND test_multi_channel_hal)

# --- Feature 7.1: Replay-Buffered Mode ---

add_executable(test_replay_buffer test_replay_buffer.cpp)
target_link_libraries(test_replay_buffer
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_replay_buffer COMMAND test_replay_buffer)

# --- Feature X.3: Calibration Interpolation ---

add_executable(test_calibration_interpolation test_calibration_interpolation.cpp)
target_link_libraries(test_calibration_interpolation
    PRIVATE
        archerfish_reporting
        Catch2::Catch2WithMain
)
add_test(NAME unit_calibration_interpolation COMMAND test_calibration_interpolation)

# --- Feature X.4: Target Power ---

add_executable(test_target_power test_target_power.cpp)
target_link_libraries(test_target_power
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_target_power COMMAND test_target_power)

# --- Feature X.5: Regulated Band Warnings ---

add_executable(test_regulatory test_regulatory.cpp)
target_link_libraries(test_regulatory
    PRIVATE
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_regulatory COMMAND test_regulatory)

# --- Feature X.6: High-Power Warning ---

add_executable(test_high_power_warning test_high_power_warning.cpp)
target_link_libraries(test_high_power_warning
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_high_power_warning COMMAND test_high_power_warning)

# --- Feature X.7: Lab-Safe Profiles ---

add_executable(test_safety_profile test_safety_profile.cpp)
target_link_libraries(test_safety_profile
    PRIVATE
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_safety_profile COMMAND test_safety_profile)

# --- Feature X.8: CI16 Format ---

add_executable(test_ci16_format test_ci16_format.cpp)
target_link_libraries(test_ci16_format
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME unit_ci16_format COMMAND test_ci16_format)

# --- Feature X.9: YAML Support ---

add_executable(test_yaml_parser test_yaml_parser.cpp)
target_link_libraries(test_yaml_parser
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_yaml_parser COMMAND test_yaml_parser)

# --- Phase X.11: DSP module standalone tests ---

add_executable(test_summer test_summer.cpp)
target_link_libraries(test_summer
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_summer COMMAND test_summer)

add_executable(test_scaler test_scaler.cpp)
target_link_libraries(test_scaler
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_scaler COMMAND test_scaler)

add_executable(test_pulse_shaper test_pulse_shaper.cpp)
target_link_libraries(test_pulse_shaper
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_pulse_shaper COMMAND test_pulse_shaper)

add_executable(test_source_factory test_source_factory.cpp)
target_link_libraries(test_source_factory
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_source_factory COMMAND test_source_factory)

add_executable(test_am_source test_am_source.cpp)
target_link_libraries(test_am_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_am_source COMMAND test_am_source)

add_executable(test_fm_source test_fm_source.cpp)
target_link_libraries(test_fm_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_fm_source COMMAND test_fm_source)

add_executable(test_pm_source test_pm_source.cpp)
target_link_libraries(test_pm_source
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_pm_source COMMAND test_pm_source)

add_executable(test_ask_source_standalone test_ask_source_standalone.cpp)
target_link_libraries(test_ask_source_standalone
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_ask_source_standalone COMMAND test_ask_source_standalone)

add_executable(test_fsk_source_standalone test_fsk_source_standalone.cpp)
target_link_libraries(test_fsk_source_standalone
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_fsk_source_standalone COMMAND test_fsk_source_standalone)

add_executable(test_constellation_standalone test_constellation_standalone.cpp)
target_link_libraries(test_constellation_standalone
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_constellation_standalone COMMAND test_constellation_standalone)

# --- Phase X.11b: Advanced DSP coverage tests ---

add_executable(test_cw_edge_cases test_cw_edge_cases.cpp)
target_link_libraries(test_cw_edge_cases
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_cw_edge_cases COMMAND test_cw_edge_cases)

add_executable(test_chirp_edge_cases test_chirp_edge_cases.cpp)
target_link_libraries(test_chirp_edge_cases
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_chirp_edge_cases COMMAND test_chirp_edge_cases)

add_executable(test_noise_statistics test_noise_statistics.cpp)
target_link_libraries(test_noise_statistics
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_noise_statistics COMMAND test_noise_statistics)

add_executable(test_modulator_advanced test_modulator_advanced.cpp)
target_link_libraries(test_modulator_advanced
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_modulator_advanced COMMAND test_modulator_advanced)

add_executable(test_multi_tone_advanced test_multi_tone_advanced.cpp)
target_link_libraries(test_multi_tone_advanced
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_multi_tone_advanced COMMAND test_multi_tone_advanced)

add_executable(test_pulse_patterns test_pulse_patterns.cpp)
target_link_libraries(test_pulse_patterns
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_pulse_patterns COMMAND test_pulse_patterns)

add_executable(test_file_source_formats test_file_source_formats.cpp)
target_link_libraries(test_file_source_formats
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_file_source_formats COMMAND test_file_source_formats)

add_executable(test_ofdm_parameters test_ofdm_parameters.cpp)
target_link_libraries(test_ofdm_parameters
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_ofdm_parameters COMMAND test_ofdm_parameters)

add_executable(test_resampler_ratios test_resampler_ratios.cpp)
target_link_libraries(test_resampler_ratios
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_resampler_ratios COMMAND test_resampler_ratios)

add_executable(test_source_lifecycle test_source_lifecycle.cpp)
target_link_libraries(test_source_lifecycle
    PRIVATE
        archerfish_dsp
        Catch2::Catch2WithMain
)
add_test(NAME dsp_source_lifecycle COMMAND test_source_lifecycle)

# --- Feature X.10: Extended Exit Codes ---

add_executable(test_exit_codes test_exit_codes.cpp)
target_link_libraries(test_exit_codes
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME unit_exit_codes COMMAND test_exit_codes)

# --- Phase X.12: Scheduler coverage tests ---

add_executable(test_parser_errors test_parser_errors.cpp)
target_link_libraries(test_parser_errors
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_parser_errors COMMAND test_parser_errors)

add_executable(test_parser_waveform_ref test_parser_waveform_ref.cpp)
target_link_libraries(test_parser_waveform_ref
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_parser_waveform_ref COMMAND test_parser_waveform_ref)

add_executable(test_parser_impairments test_parser_impairments.cpp)
target_link_libraries(test_parser_impairments
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_parser_impairments COMMAND test_parser_impairments)

add_executable(test_planner_multi_channel test_planner_multi_channel.cpp)
target_link_libraries(test_planner_multi_channel
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_planner_multi_channel COMMAND test_planner_multi_channel)

add_executable(test_plan_serialization test_plan_serialization.cpp)
target_link_libraries(test_plan_serialization
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_plan_serialization COMMAND test_plan_serialization)

add_executable(test_scenario_roundtrip test_scenario_roundtrip.cpp)
target_link_libraries(test_scenario_roundtrip
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_scenario_roundtrip COMMAND test_scenario_roundtrip)

add_executable(test_scenario_validate_plan test_scenario_validate_plan.cpp)
target_link_libraries(test_scenario_validate_plan
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_scenario_validate_plan COMMAND test_scenario_validate_plan)

# --- Phase X.13: Reporting coverage tests ---

add_executable(test_calibration_roundtrip test_calibration_roundtrip.cpp)
target_link_libraries(test_calibration_roundtrip
    PRIVATE
        archerfish_reporting
        Catch2::Catch2WithMain
)
add_test(NAME unit_calibration_roundtrip COMMAND test_calibration_roundtrip)

add_executable(test_metrics_format test_metrics_format.cpp)
target_link_libraries(test_metrics_format
    PRIVATE
        archerfish_reporting
        Catch2::Catch2WithMain
)
add_test(NAME unit_metrics_format COMMAND test_metrics_format)

add_executable(test_report_generation test_report_generation.cpp)
target_link_libraries(test_report_generation
    PRIVATE
        archerfish_reporting
        Catch2::Catch2WithMain
)
add_test(NAME unit_report_generation COMMAND test_report_generation)

# --- Phase X.14: Runtime coverage tests ---

add_executable(test_runtime_lifecycle test_runtime_lifecycle.cpp)
target_link_libraries(test_runtime_lifecycle
    PRIVATE
        archerfish_runtime
        archerfish_hal
        Catch2::Catch2WithMain
)
add_test(NAME unit_runtime_lifecycle COMMAND test_runtime_lifecycle)

add_executable(test_runtime_multi_channel test_runtime_multi_channel.cpp)
target_link_libraries(test_runtime_multi_channel
    PRIVATE
        archerfish_runtime
        archerfish_hal
        Catch2::Catch2WithMain
)
add_test(NAME unit_runtime_multi_channel COMMAND test_runtime_multi_channel)

add_executable(test_event_dispatcher_timing test_event_dispatcher_timing.cpp)
target_link_libraries(test_event_dispatcher_timing
    PRIVATE
        archerfish_runtime
        Catch2::Catch2WithMain
)
add_test(NAME unit_event_dispatcher_timing COMMAND test_event_dispatcher_timing)

add_executable(test_render_worker_jobs test_render_worker_jobs.cpp)
target_link_libraries(test_render_worker_jobs
    PRIVATE
        archerfish_runtime
        Catch2::Catch2WithMain
)
add_test(NAME unit_render_worker_jobs COMMAND test_render_worker_jobs)

# --- Phase X.15: CLI coverage tests ---

add_executable(test_cli_subcommands test_cli_subcommands.cpp)
target_link_libraries(test_cli_subcommands
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME unit_cli_subcommands COMMAND test_cli_subcommands)

add_executable(test_exit_codes_extended test_exit_codes_extended.cpp)
target_link_libraries(test_exit_codes_extended
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME unit_exit_codes_extended COMMAND test_exit_codes_extended)

# --- Phase X.16: Impairment coverage tests ---

add_executable(test_impairment_pipeline test_impairment_pipeline.cpp)
target_link_libraries(test_impairment_pipeline
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_impairment_pipeline COMMAND test_impairment_pipeline)

# --- Feature X.11: Metrics Export --latest ---

add_executable(test_metrics_export test_metrics_export.cpp)
target_link_libraries(test_metrics_export
    PRIVATE
        archerfish_cli
        Catch2::Catch2WithMain
)
add_test(NAME unit_metrics_export COMMAND test_metrics_export)

# --- Phase 8.1: Standalone Impairment Tests ---

add_executable(test_cfo_standalone test_cfo_standalone.cpp)
target_link_libraries(test_cfo_standalone
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_cfo_standalone COMMAND test_cfo_standalone)

add_executable(test_awgn_standalone test_awgn_standalone.cpp)
target_link_libraries(test_awgn_standalone
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_awgn_standalone COMMAND test_awgn_standalone)

add_executable(test_phase_offset_standalone test_phase_offset_standalone.cpp)
target_link_libraries(test_phase_offset_standalone
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_phase_offset_standalone COMMAND test_phase_offset_standalone)

add_executable(test_iq_imbalance_standalone test_iq_imbalance_standalone.cpp)
target_link_libraries(test_iq_imbalance_standalone
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_iq_imbalance_standalone COMMAND test_iq_imbalance_standalone)

add_executable(test_dc_offset_standalone test_dc_offset_standalone.cpp)
target_link_libraries(test_dc_offset_standalone
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_dc_offset_standalone COMMAND test_dc_offset_standalone)

add_executable(test_amplitude_ripple_standalone test_amplitude_ripple_standalone.cpp)
target_link_libraries(test_amplitude_ripple_standalone
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_amplitude_ripple_standalone COMMAND test_amplitude_ripple_standalone)

add_executable(test_delay_standalone test_delay_standalone.cpp)
target_link_libraries(test_delay_standalone
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_delay_standalone COMMAND test_delay_standalone)

add_executable(test_burst_dropout_standalone test_burst_dropout_standalone.cpp)
target_link_libraries(test_burst_dropout_standalone
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_burst_dropout_standalone COMMAND test_burst_dropout_standalone)

# --- Phase 8.2: Additional Impairment Coverage ---

add_executable(test_impairment_chain_ordering test_impairment_chain_ordering.cpp)
target_link_libraries(test_impairment_chain_ordering
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_impairment_chain_ordering COMMAND test_impairment_chain_ordering)

add_executable(test_impairment_enable_disable test_impairment_enable_disable.cpp)
target_link_libraries(test_impairment_enable_disable
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_impairment_enable_disable COMMAND test_impairment_enable_disable)

add_executable(test_phase_noise_parameters test_phase_noise_parameters.cpp)
target_link_libraries(test_phase_noise_parameters
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_phase_noise_parameters COMMAND test_phase_noise_parameters)

add_executable(test_multipath_taps test_multipath_taps.cpp)
target_link_libraries(test_multipath_taps
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_multipath_taps COMMAND test_multipath_taps)

add_executable(test_fading_models test_fading_models.cpp)
target_link_libraries(test_fading_models
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_fading_models COMMAND test_fading_models)

add_executable(test_pa_rapp_vs_saleh test_pa_rapp_vs_saleh.cpp)
target_link_libraries(test_pa_rapp_vs_saleh
    PRIVATE
        archerfish_impairments
        archerfish_common
        Catch2::Catch2WithMain
)
add_test(NAME unit_pa_rapp_vs_saleh COMMAND test_pa_rapp_vs_saleh)

# --- Phase 8.3: Validator Edge Case Tests ---

add_executable(test_validator_channels test_validator_channels.cpp)
target_link_libraries(test_validator_channels
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_validator_channels COMMAND test_validator_channels)

add_executable(test_validator_sync_groups test_validator_sync_groups.cpp)
target_link_libraries(test_validator_sync_groups
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_validator_sync_groups COMMAND test_validator_sync_groups)

add_executable(test_validator_events test_validator_events.cpp)
target_link_libraries(test_validator_events
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_validator_events COMMAND test_validator_events)

add_executable(test_validator_regulatory test_validator_regulatory.cpp)
target_link_libraries(test_validator_regulatory
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_validator_regulatory COMMAND test_validator_regulatory)

add_executable(test_validator_safety test_validator_safety.cpp)
target_link_libraries(test_validator_safety
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_validator_safety COMMAND test_validator_safety)

add_executable(test_validator_multi_emitter test_validator_multi_emitter.cpp)
target_link_libraries(test_validator_multi_emitter
    PRIVATE
        archerfish_scheduler
        Catch2::Catch2WithMain
)
add_test(NAME unit_validator_multi_emitter COMMAND test_validator_multi_emitter)




