# Archerfish MVP Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a CLI-first programmable vector signal generator for USRP, supporting CW/chirp/noise/QPSK/QAM/IQ-replay waveforms with scenario-driven execution, validation, planning, and reporting.

**Architecture:** Layered C++23 application with clean boundaries between CLI, scenario processing (parse->validate->plan->execute), DSP/waveform generation, HAL (UHD abstraction), and runtime (threaded TX streaming). All layers communicate via typed data structures.

**Tech Stack:** C++23, CMake, Conan 2, CLI11, spdlog, fmt, nlohmann/json, nlohmann/json-schema-validator, libsamplerate, Catch2, UHD

**Resolved Open Questions:**
- Resampler: libsamplerate via Conan
- Overlapping emitters: reject all overlap in v1
- JSON schema validator: nlohmann/json-schema
- Overlap composition: deferred to Phase 2

---

## Increment 1: Build System Skeleton

### Task 1.1: Repository structure

**Files:**
- Create: `CMakeLists.txt`
- Create: `conanfile.py`
- Create: `src/CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `core/CMakeLists.txt`
- Create: `core/hal/CMakeLists.txt`
- Create: `core/runtime/CMakeLists.txt`
- Create: `core/dsp/CMakeLists.txt`
- Create: `core/impairments/CMakeLists.txt`
- Create: `core/scheduler/CMakeLists.txt`
- Create: `core/reporting/CMakeLists.txt`
- Create: `core/common/CMakeLists.txt`
- Create: `cli/CMakeLists.txt`
- Create: `schemas/CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Create: `tests/unit/CMakeLists.txt`
- Create: `tests/integration/CMakeLists.txt`
- Create: `examples/` directory

**Step 1:** Create directory structure:

```
archerfish/
├── CMakeLists.txt
├── conanfile.py
├── src/
│   └── main.cpp
├── core/
│   ├── CMakeLists.txt
│   ├── common/
│   ├── hal/
│   ├── runtime/
│   ├── dsp/
│   ├── impairments/
│   ├── scheduler/
│   └── reporting/
├── cli/
├── schemas/
├── tests/
│   ├── unit/
│   └── integration/
└── examples/
```

**Step 2:** Write `conanfile.py` with dependencies: cli11/2.*, spdlog/1.*, fmt/10.*, nlohmann_json/3.*, libsamplerate/0.*, catch2/3.*. UHD handled via system install + CMake find_package.

**Step 3:** Write top-level `CMakeLists.txt`:
- `cmake_minimum_required(VERSION 3.25)`
- `project(archerfish VERSION 0.1.0 LANGUAGES CXX)`
- `set(CMAKE_CXX_STANDARD 23)`
- Add subdirectories: core, cli, src, tests
- Use `find_package` for Conan-generated deps

**Step 4:** Write `src/main.cpp`:

```cpp
#include <fmt/format.h>
int main() {
    fmt::print("archerfish v0.1.0\n");
    return 0;
}
```

**Step 5:** `conan install . --build=missing && cmake --preset release && cmake --build --preset release`

**Step 6:** Run `./build/archerfish` -> expect "archerfish v0.1.0"

**Step 7:** Commit: `feat: project skeleton with CMake/Conan build system`

---

### Task 1.2: Catch2 test infrastructure

**Files:**
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/test_main.cpp`

**Step 1:** Configure Catch2 with CTest integration in `tests/CMakeLists.txt`.

**Step 2:** Write `tests/unit/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
TEST_CASE("build sanity") { REQUIRE(true); }
```

**Step 3:** Build and run tests -> expect 1 passing test.

**Step 4:** Commit: `feat: add Catch2 test infrastructure`

---

## Increment 2: Core Common Types

### Task 2.1: Error types and categories

**Files:**
- Create: `core/common/include/archerfish/common/error.hpp`
- Create: `core/common/src/error.cpp`
- Create: `tests/unit/test_error.cpp`

**Step 1:** Write test for `Error` and `ErrorCategory` enum covering: Config, Validation, Planning, Preparation, Execution, QualityWarning.

**Step 2:** Write `error.hpp` with:

```cpp
enum class ErrorCategory { Config, Validation, Planning, Preparation, Execution, QualityWarning };
struct Error { ErrorCategory category; std::string code; std::string message; };
using ErrorList = std::vector<Error>;
```

**Step 3:** Build and test.

**Step 4:** Commit.

---

### Task 2.2: RF and timing types

**Files:**
- Create: `core/common/include/archerfish/common/rf_types.hpp`
- Create: `core/common/include/archerfish/common/types.hpp`
- Create: `tests/unit/test_rf_types.cpp`

**Step 1:** Write test for `RfConfig` (freq_hz, rate_sps, gain_db, bandwidth_hz, antenna), `TimeSpec` (seconds + fractional nanoseconds), `Duration`, `ChannelId`, `DeviceId`.

**Step 2:** Implement typed structs with validation methods (e.g., `is_valid()` returning `ErrorList`).

**Step 3:** Build and test.

**Step 4:** Commit: `feat: core RF and timing type definitions`

---

### Task 2.3: Sample format types

**Files:**
- Create: `core/common/include/archerfish/common/sample.hpp`
- Create: `tests/unit/test_sample.cpp`

**Step 1:** Define `SampleFormat` enum (CF32, CI16, CI8), `SampleBuffer` struct wrapping `std::vector<std::complex<float>>` with metadata (sample_rate, count).

**Step 2:** Build and test.

**Step 3:** Commit.

---

## Increment 3: HAL Interface + Stub

### Task 3.1: HAL abstract interface

**Files:**
- Create: `core/hal/include/archerfish/hal/hal_device.hpp`
- Create: `core/hal/include/archerfish/hal/hal_factory.hpp`
- Create: `core/hal/include/archerfish/hal/device_capabilities.hpp`
- Create: `tests/unit/test_hal_interface.cpp`

**Step 1:** Write tests defining expected HAL behavior.

**Step 2:** Implement `IHalDevice` abstract interface matching Design.md S11.3:

```cpp
class IHalDevice {
public:
    virtual ~IHalDevice() = default;
    virtual DeviceCapabilities get_capabilities() const = 0;
    virtual void set_center_freq(uint32_t channel, double hz) = 0;
    virtual void set_sample_rate(uint32_t channel, double sps) = 0;
    virtual void set_bandwidth(uint32_t channel, double hz) = 0;
    virtual void set_gain(uint32_t channel, double db) = 0;
    virtual void set_antenna(uint32_t channel, std::string_view port) = 0;
    virtual void set_clock_source(std::string_view source) = 0;
    virtual void set_time_source(std::string_view source) = 0;
    virtual void sync_time_now() = 0;
    // TX control
    virtual void start_tx(uint32_t channel) = 0;
    virtual void stop_tx(uint32_t channel) = 0;
    // Streaming
    virtual void send_samples(uint32_t channel, const std::complex<float>* data, size_t count, const TxMetadata& meta) = 0;
};
```

**Step 3:** Implement `DeviceCapabilities` struct with: channels, freq_range, rate_range, gain_range, bandwidth_range, supported_clock_sources, supported_time_sources.

**Step 4:** Build and test.

**Step 5:** Commit: `feat: HAL abstract interface and device capabilities`

---

### Task 3.2: HAL stub implementation

**Files:**
- Create: `core/hal/include/archerfish/hal/stub_device.hpp`
- Create: `core/hal/src/stub_device.cpp`
- Create: `tests/unit/test_hal_stub.cpp`

**Step 1:** Write tests that exercise stub HAL -- verify all setters record state, `send_samples` counts samples, `start_tx`/`stop_tx` track state.

**Step 2:** Implement `StubDevice` with configurable capabilities and recorded call history (for test inspection).

**Step 3:** Build and test.

**Step 4:** Commit: `feat: HAL stub device for testing`

---

### Task 3.3: HAL UHD implementation

**Files:**
- Create: `core/hal/include/archerfish/hal/uhd_device.hpp`
- Create: `core/hal/src/uhd_device.cpp`
- Create: `tests/hardware/test_uhd_device.cpp`

**Step 1:** Implement `UhdDevice` wrapping `uhd::usrp::multi_usrp`. Guarded behind `ARCHERFISH_HAS_UHD` cmake option so builds without UHD installed still compile (stub-only mode).

**Step 2:** Implement device enumeration via `uhd::device::find()`.

**Step 3:** Implement all `IHalDevice` methods delegating to UHD API.

**Step 4:** Hardware test (only runs if USRP connected). Commit.

---

## Increment 4: DSP Waveform Generators

### Task 4.1: Source interface and block model

**Files:**
- Create: `core/dsp/include/archerfish/dsp/source.hpp`
- Create: `core/dsp/include/archerfish/dsp/block_io.hpp`
- Create: `tests/unit/test_source_interface.cpp`

**Step 1:** Define `ISource` interface:

```cpp
struct WaveformMetadata {
    double nominal_bandwidth;
    double sample_rate;
    double peak_amplitude;
    double rms_amplitude;
    double crest_factor;
    std::optional<double> duration_sec;
    bool repeats;
};

class ISource {
public:
    virtual ~ISource() = default;
    virtual void configure(const nlohmann::json& params) = 0;
    virtual void prepare() = 0;
    virtual size_t render_block(std::complex<float>* out, size_t max_samples) = 0;
    virtual WaveformMetadata report_metadata() const = 0;
};
```

**Step 2:** Test interface contract.

**Step 3:** Commit.

---

### Task 4.2: CW generator

**Files:**
- Create: `core/dsp/include/archerfish/dsp/cw_source.hpp`
- Create: `core/dsp/src/cw_source.cpp`
- Create: `tests/unit/test_cw_source.cpp`

**Step 1:** Write tests: CW at 0 Hz produces constant amplitude, CW at F Hz produces expected phase progression, metadata is correct, block rendering works across boundaries.

**Step 2:** Implement `CwSource`: generates `amplitude * exp(j*2*pi*freq/sps * n)`.

**Step 3:** Build and test. Commit.

---

### Task 4.3: Chirp generator

**Files:**
- Create: `core/dsp/include/archerfish/dsp/chirp_source.hpp`
- Create: `core/dsp/src/chirp_source.cpp`
- Create: `tests/unit/test_chirp_source.cpp`

**Step 1:** Test chirp from f0 to f1 over duration, verify start/end frequency, verify amplitude.

**Step 2:** Implement linear FM chirp: instantaneous frequency sweeps linearly from f0 to f1.

**Step 3:** Build and test. Commit.

---

### Task 4.4: Noise generator

**Files:**
- Create: `core/dsp/include/archerfish/dsp/noise_source.hpp`
- Create: `core/dsp/src/noise_source.cpp`
- Create: `tests/unit/test_noise_source.cpp`

**Step 1:** Test noise amplitude statistics (mean ~0, variance ~amplitude^2/2 per component), seeded reproducibility.

**Step 2:** Implement AWGN using a seeded PRNG (e.g., std::mt19937 + normal distribution).

**Step 3:** Build and test. Commit.

---

### Task 4.5: Digital modulator (PSK/QAM)

**Files:**
- Create: `core/dsp/include/archerfish/dsp/modulator.hpp`
- Create: `core/dsp/include/archerfish/dsp/pulse_shaper.hpp`
- Create: `core/dsp/src/modulator.cpp`
- Create: `core/dsp/src/pulse_shaper.cpp`
- Create: `tests/unit/test_modulator.cpp`

**Step 1:** Test constellation mapping for BPSK, QPSK, 16QAM, verify symbol values.

**Step 2:** Test RRC pulse shaping (alpha parameter, taps, ISI properties).

**Step 3:** Implement `ModulatorSource`: random bits -> constellation mapper -> upsampling -> RRC filter -> output. Supports BPSK/QPSK/8PSK/16QAM/64QAM.

**Step 4:** Build and test. Commit: `feat: PSK/QAM modulator with RRC pulse shaping`

---

### Task 4.6: Multi-tone generator

**Files:**
- Create: `core/dsp/include/archerfish/dsp/multi_tone_source.hpp`
- Create: `core/dsp/src/multi_tone_source.cpp`
- Create: `tests/unit/test_multi_tone.cpp`

**Step 1:** Test sum of N tones at specified frequencies/amplitudes, verify spectral content.

**Step 2:** Implement: sum of `An * exp(j*2*pi*fn/sps * n)` for each tone.

**Step 3:** Build and test. Commit.

---

### Task 4.7: IQ file replay source

**Files:**
- Create: `core/dsp/include/archerfish/dsp/file_source.hpp`
- Create: `core/dsp/src/file_source.cpp`
- Create: `tests/unit/test_file_source.cpp`
- Create: `tests/fixtures/` (test IQ files)

**Step 1:** Test reading CF32 binary file, looping, finite replay, EOF behavior.

**Step 2:** Implement `FileSource`: reads complex<float> from binary file, supports loop/finite modes, reads optional sidecar JSON for sample rate metadata.

**Step 3:** Build and test. Commit.

---

### Task 4.8: Processing blocks (scaler, summer)

**Files:**
- Create: `core/dsp/include/archerfish/dsp/scaler.hpp`
- Create: `core/dsp/include/archerfish/dsp/summer.hpp`
- Create: `tests/unit/test_processing_blocks.cpp`

**Step 1:** Test scaler (multiply by complex or real factor), test summer (add two buffers element-wise with headroom check).

**Step 2:** Implement.

**Step 3:** Build and test. Commit.

---

## Increment 5: Impairment Blocks

### Task 5.1: Impairment interface

**Files:**
- Create: `core/impairments/include/archerfish/impairments/impairment.hpp`
- Create: `tests/unit/test_impairment_interface.cpp`

**Step 1:** Define `IImpairment`:

```cpp
class IImpairment {
public:
    virtual ~IImpairment() = default;
    virtual void apply(std::complex<float>* data, size_t count) = 0;
    virtual std::string name() const = 0;
    virtual bool enabled() const = 0;
    virtual void set_enabled(bool v) = 0;
};
```

**Step 2:** Test interface. Commit.

---

### Task 5.2: Implement initial impairments

**Files:**
- Create: `core/impairments/include/archerfish/impairments/awgn.hpp`
- Create: `core/impairments/include/archerfish/impairments/cfo.hpp`
- Create: `core/impairments/include/archerfish/impairments/phase_offset.hpp`
- Create: `core/impairments/include/archerfish/impairments/iq_imbalance.hpp`
- Create: `core/impairments/include/archerfish/impairments/dc_offset.hpp`
- Create: `core/impairments/src/awgn.cpp` (and others)
- Create: `tests/unit/test_impairments.cpp`

**Step 1:** Write tests for each impairment:
- AWGN: adds noise at specified SNR
- CFO: applies `exp(j*2*pi*cfo_hz/sps * n)` rotation
- Phase offset: `exp(j*phase_rad)`
- IQ gain/phase imbalance: `I' = (1+g)*I, Q' = (1-g)*sin(p)*I + (1-g)*cos(p)*Q`
- DC offset: `+ (dc_i + j*dc_q)`

**Step 2:** Implement all impairments.

**Step 3:** Build and test. Commit: `feat: initial impairment set (AWGN, CFO, IQ imbalance, DC offset)`

---

### Task 5.3: Impairment chain

**Files:**
- Create: `core/impairments/include/archerfish/impairments/impairment_chain.hpp`
- Create: `core/impairments/src/impairment_chain.cpp`
- Create: `tests/unit/test_impairment_chain.cpp`

**Step 1:** Test ordered application of multiple impairments, enable/disable individual impairments.

**Step 2:** Implement `ImpairmentChain` as ordered list of `unique_ptr<IImpairment>`.

**Step 3:** Build and test. Commit.

---

## Increment 6: Scenario Data Model + Parser

### Task 6.1: Scenario data model

**Files:**
- Create: `core/scheduler/include/archerfish/scenario/scenario.hpp`
- Create: `tests/unit/test_scenario_model.cpp`

**Step 1:** Define the full scenario data model as typed structs:

```cpp
struct Metadata { std::string name; };
struct RfConfig { double freq_hz, rate_sps, gain_db; std::optional<double> bandwidth_hz; std::optional<std::string> antenna; };
struct DeviceDef { std::string id; std::optional<uint32_t> channel; RfConfig rf; };
struct WaveformDef { std::optional<std::string> id; std::string type; nlohmann::json params; };
struct Impairments { std::optional<double> cfo_hz; std::optional<double> iq_gain_imbalance_db; /* ... */ };
struct EmitterDef { std::string id; std::string device; uint32_t channel; double start_after_sec; double duration_sec; std::optional<WaveformDef> waveform; std::optional<std::string> waveform_ref; std::optional<Impairments> impairments; };
struct ReportingConfig { bool save_plan{false}; bool save_metrics{false}; };
struct Scenario { Metadata metadata; std::vector<DeviceDef> devices; std::vector<WaveformDef> waveforms; std::vector<EmitterDef> emitters; ReportingConfig reporting; };
```

**Step 2:** Test default construction, field access.

**Step 3:** Commit.

---

### Task 6.2: Scenario JSON parser

**Files:**
- Create: `core/scheduler/include/archerfish/scenario/parser.hpp`
- Create: `core/scheduler/src/parser.cpp`
- Create: `tests/unit/test_parser.cpp`
- Create: `examples/future_start_cw.json`
- Create: `examples/chirp_burst.json`
- Create: `examples/qpsk_burst.json`

**Step 1:** Write tests parsing the three example scenarios from Design.md S30, verify all fields parsed correctly.

**Step 2:** Write tests for waveform_ref resolution (inline vs named).

**Step 3:** Implement `parse_scenario(const std::filesystem::path& json_path) -> expected<Scenario, ErrorList>`.

**Step 4:** Build and test. Commit: `feat: scenario JSON parser with example files`

---

## Increment 7: Scenario Schema + Validator

### Task 7.1: JSON Schema definition

**Files:**
- Create: `schemas/scenario.schema.json`
- Create: `tests/unit/test_schema_validation.cpp`

**Step 1:** Write JSON Schema covering: metadata, devices (required: id, rf.freq_hz, rf.rate_sps, rf.gain_db), waveforms (required: id, type), emitters (required: id, device, channel, start_after_sec, duration_sec, waveform or waveform_ref).

**Step 2:** Test schema validates good scenarios and rejects malformed ones.

**Step 3:** Commit.

---

### Task 7.2: Semantic validator

**Files:**
- Create: `core/scheduler/include/archerfish/scenario/validator.hpp`
- Create: `core/scheduler/src/validator.cpp`
- Create: `tests/unit/test_validator.cpp`

**Step 1:** Write tests for:
- amplitude range (0, 1] -> reject amplitude > 1.0 or <= 0
- duration > 0
- referenced device/waveform IDs exist
- referenced channel is valid for device
- **overlapping emitters on same channel -> REJECT** (v1 policy)
- digital headroom check: sum of amplitudes <= 1.0

**Step 2:** Implement `validate(const Scenario& scenario, const DeviceCapabilities& caps) -> ErrorList`.

**Step 3:** Build and test. Commit: `feat: scenario semantic validator with overlap rejection`

---

## Increment 8: Planner

### Task 8.1: Plan data model

**Files:**
- Create: `core/scheduler/include/archerfish/scenario/plan.hpp`
- Create: `tests/unit/test_plan_model.cpp`

**Step 1:** Define plan structs:

```cpp
struct ChannelBinding { std::string device_id; uint32_t channel_index; RfConfig rf; };
struct TimelineEvent { enum Type { EmitterStart, EmitterStop, GainChange, FreqChange, Marker }; Type type; double time_sec; std::string target_id; nlohmann::json payload; };
struct RenderInstruction { std::string emitter_id; WaveformDef waveform; double start_sec; double duration_sec; double sample_rate; std::optional<double> resample_ratio; };
struct Plan { Scenario normalized_scenario; std::vector<ChannelBinding> channels; std::vector<TimelineEvent> timeline; std::vector<RenderInstruction> render_instructions; ErrorList warnings; };
```

**Step 2:** Test. Commit.

---

### Task 8.2: Planner implementation

**Files:**
- Create: `core/scheduler/include/archerfish/scenario/planner.hpp`
- Create: `core/scheduler/src/planner.cpp`
- Create: `tests/unit/test_planner.cpp`

**Step 1:** Write tests:
- Single emitter -> correct timeline (start event, stop event)
- Multiple emitters on different channels -> separate timeline events
- Waveform rate == device rate -> no resampling
- Waveform rate != device rate -> resampling decision recorded
- All timing resolved to absolute offsets from epoch

**Step 2:** Implement `plan(const Scenario& scenario) -> expected<Plan, ErrorList>`.

**Step 3:** Build and test. Commit: `feat: scenario planner with timeline and resampling decisions`

---

### Task 8.3: Plan serialization

**Files:**
- Create: `core/scheduler/include/archerfish/scenario/plan_io.hpp`
- Create: `core/scheduler/src/plan_io.cpp`
- Create: `tests/unit/test_plan_io.cpp`

**Step 1:** Test `Plan` -> JSON round-trip (all fields preserved).

**Step 2:** Implement `plan_to_json(const Plan&)` and `plan_from_json(const json&)`.

**Step 3:** Build and test. Commit.

---

## Increment 9: Runtime Engine

### Task 9.1: Runtime state machine

**Files:**
- Create: `core/runtime/include/archerfish/runtime/state.hpp`
- Create: `core/runtime/src/state.cpp`
- Create: `tests/unit/test_runtime_state.cpp`

**Step 1:** Test state transitions: Created->Validated->Planned->Prepared->Armed->Running->Completed, and error transitions to Failed/Aborted.

**Step 2:** Implement `RuntimeState` enum and `StateMachine` with valid-transition checking.

**Step 3:** Build and test. Commit.

---

### Task 9.2: Bounded SPSC queue

**Files:**
- Create: `core/runtime/include/archerfish/runtime/spsc_queue.hpp`
- Create: `tests/unit/test_spsc_queue.cpp`

**Step 1:** Test: single-producer push, single-consumer pop, backpressure when full, empty returns nullopt.

**Step 2:** Implement lock-free bounded SPSC queue for `SampleBlock` (wrapping `std::vector<std::complex<float>>`).

**Step 3:** Build and test. Commit.

---

### Task 9.3: Render thread

**Files:**
- Create: `core/runtime/include/archerfish/runtime/render_worker.hpp`
- Create: `core/runtime/src/render_worker.cpp`
- Create: `tests/unit/test_render_worker.cpp`

**Step 1:** Test: render worker produces correct sample blocks for a CW emitter, respects duration, signals completion.

**Step 2:** Implement `RenderWorker`: takes `RenderInstruction`, uses appropriate `ISource`, pushes blocks into SPSC queue.

**Step 3:** Build and test. Commit.

---

### Task 9.4: TX worker thread

**Files:**
- Create: `core/runtime/include/archerfish/runtime/tx_worker.hpp`
- Create: `core/runtime/src/tx_worker.cpp`
- Create: `tests/unit/test_tx_worker.cpp`

**Step 1:** Test: TX worker reads from queue, sends to HAL, records metrics (samples sent, underruns).

**Step 2:** Implement `TxWorker`: reads blocks from SPSC queue, calls `IHalDevice::send_samples()`, tracks metrics via atomics.

**Step 3:** Build and test. Commit.

---

### Task 9.5: Event dispatcher thread

**Files:**
- Create: `core/runtime/include/archerfish/runtime/event_dispatcher.hpp`
- Create: `core/runtime/src/event_dispatcher.cpp`
- Create: `tests/unit/test_event_dispatcher.cpp`

**Step 1:** Test: dispatcher fires events at correct wall-clock offsets, records dispatch status.

**Step 2:** Implement `EventDispatcher`: sleeps until event time, fires callback, records completion.

**Step 3:** Build and test. Commit.

---

### Task 9.6: Runtime coordinator

**Files:**
- Create: `core/runtime/include/archerfish/runtime/runtime.hpp`
- Create: `core/runtime/src/runtime.cpp`
- Create: `tests/integration/test_runtime.cpp`

**Step 1:** Integration test: using `StubDevice`, run a CW scenario end-to-end (prepare -> arm -> run -> complete), verify metrics collected.

**Step 2:** Implement `Runtime` coordinator that orchestrates state machine, threads, and HAL.

**Step 3:** Build and test. Commit: `feat: runtime engine with threaded render/TX/event pipeline`

---

## Increment 10: CLI Commands

### Task 10.1: CLI framework setup

**Files:**
- Create: `cli/include/archerfish/cli/app.hpp`
- Create: `cli/src/app.cpp`
- Create: `tests/unit/test_cli_app.cpp`

**Step 1:** Wire CLI11 with subcommand groups: `devices`, `scenario`, `wave`, `calib`, `report`, plus top-level `doctor` and `version`.

**Step 2:** Add `--json`, `--verbose`, `--quiet` global flags.

**Step 3:** Define exit code constants per Design.md S16.3.

**Step 4:** Build and test. Commit.

---

### Task 10.2: `devices list` and `devices info`

**Files:**
- Create: `cli/src/cmd_devices.cpp`
- Create: `tests/unit/test_cmd_devices.cpp`

**Step 1:** Test: `devices list` with stub HAL returns formatted table; `--json` returns JSON array.

**Step 2:** Test: `devices info --device usrp0` returns capability details.

**Step 3:** Implement using HAL factory.

**Step 4:** Build and test. Commit.

---

### Task 10.3: `scenario validate`, `plan`, `run`

**Files:**
- Create: `cli/src/cmd_scenario.cpp`
- Create: `tests/integration/test_cmd_scenario.cpp`

**Step 1:** Test `scenario validate` returns exit code 0 on valid, 2 on validation error.

**Step 2:** Test `scenario plan` outputs human-readable summary and JSON plan.

**Step 3:** Test `scenario run` executes end-to-end with stub HAL and writes report.

**Step 4:** Implement: parse -> validate -> plan -> (for run: prepare -> execute -> report).

**Step 5:** Build and test. Commit: `feat: scenario validate/plan/run CLI commands`

---

### Task 10.4: `wave gen` and `wave inspect`

**Files:**
- Create: `cli/src/cmd_wave.cpp`
- Create: `tests/integration/test_cmd_wave.cpp`

**Step 1:** Test `wave gen cw --rate 10e6 --duration 0.1 --amplitude 0.2 -o cw.cf32` produces file with correct sample count.

**Step 2:** Test `wave gen chirp ...` and `wave gen qpsk ...`.

**Step 3:** Test `wave inspect cw.cf32` reports correct metadata.

**Step 4:** Implement using DSP sources + file I/O.

**Step 5:** Build and test. Commit: `feat: wave gen and wave inspect CLI commands`

---

### Task 10.5: `doctor` and `version`

**Files:**
- Create: `cli/src/cmd_doctor.cpp`

**Step 1:** Test `version` prints version string.

**Step 2:** Test `doctor` checks: UHD availability, device connectivity, Conan deps.

**Step 3:** Implement. Commit.

---

## Increment 11: Reporting

### Task 11.1: Report data model and output

**Files:**
- Create: `core/reporting/include/archerfish/reporting/report.hpp`
- Create: `core/reporting/include/archerfish/reporting/metrics.hpp`
- Create: `core/reporting/src/report.cpp`
- Create: `core/reporting/src/metrics.cpp`
- Create: `tests/unit/test_report.cpp`

**Step 1:** Define `Metrics` struct (Design.md S19.2): start_requested, start_actual, stop_actual, tx_duration, underrun_count, late_cmd_count, queue_depth_stats, warning_count, error_count.

**Step 2:** Define `Report` struct: scenario_hash, status, timing, device_info, warnings, artifact_paths.

**Step 3:** Test JSON serialization. Commit.

---

### Task 11.2: Run directory management

**Files:**
- Create: `core/reporting/include/archerfish/reporting/run_directory.hpp`
- Create: `core/reporting/src/run_directory.cpp`
- Create: `tests/unit/test_run_directory.cpp`

**Step 1:** Test: creates `runs/YYYY-MM-DDTHHMMSSZ_<name>/` with sub-files (scenario.normalized.json, plan.json, report.json, metrics.json, logs.txt).

**Step 2:** Implement `RunDirectory` that manages output layout.

**Step 3:** Build and test. Commit: `feat: reporting with run directory management`

---

### Task 11.3: `report show` command

**Files:**
- Modify: `cli/src/cmd_scenario.cpp`

**Step 1:** Test `report show <run_dir>` prints summary.

**Step 2:** Implement. Commit.

---

## Increment 12: Integration Tests + Examples

### Task 12.1: End-to-end integration tests

**Files:**
- Create: `tests/integration/test_e2e_cw.cpp`
- Create: `tests/integration/test_e2e_chirp.cpp`
- Create: `tests/integration/test_e2e_qpsk.cpp`
- Create: `tests/integration/test_e2e_replay.cpp`

**Step 1:** Each test: parse example scenario -> validate -> plan -> run (stub) -> verify report written -> verify metrics sane.

**Step 2:** Commit.

---

### Task 12.2: Example scenarios

**Files:**
- Verify: `examples/future_start_cw.json`
- Verify: `examples/chirp_burst.json`
- Verify: `examples/qpsk_burst.json`
- Create: `examples/mixed_scene.json`

**Step 1:** Ensure all examples pass validation and produce valid plans.

**Step 2:** Commit.

---

## Increment 13: Build Polish

### Task 13.1: CMake install target

**Step 1:** Add `install(TARGETS archerfish ...)` and configure CPack for DEB/RPM.

**Step 2:** Verify `cmake --install` works. Commit.

---

### Task 13.2: CI configuration

**Step 2:** Create GitHub Actions workflow: Conan install -> CMake build -> CTest.

**Step 3:** Commit.

---

## Summary of Build Order

| Inc | Layer | Key Deliverable | Tests |
|-----|-------|----------------|-------|
| 1 | Skeleton | CMake+Conan, hello world | 1 sanity test |
| 2 | Common | Error/RF/timing/sample types | Unit tests |
| 3 | HAL | IHalDevice, StubDevice, UhdDevice | Unit + HW tests |
| 4 | DSP | CW/chirp/noise/mod/multi-tone/file sources | Unit tests with golden checks |
| 5 | Impairments | AWGN/CFO/IQ/DC + chain | Unit tests |
| 6 | Parser | Scenario model + JSON parser | Unit tests with examples |
| 7 | Validator | Schema + semantic validation | Unit tests |
| 8 | Planner | Plan model + timeline + serialization | Unit tests |
| 9 | Runtime | State machine + 4 threads + coordinator | Unit + integration |
| 10 | CLI | All commands | Unit + integration |
| 11 | Reporting | Report/metrics + run directory | Unit tests |
| 12 | Integration | E2E tests + examples | Integration tests |
| 13 | Polish | Install target + CI | Build-only |
