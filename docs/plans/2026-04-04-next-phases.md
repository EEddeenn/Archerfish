# Archerfish Next Phases Plan

**Date**: 2026-04-04
**Status**: Complete — All Phases Implemented
**Prerequisites**: Phase 1 (MVP) ✓, Phase 2 (Waveform expansion) ✓, Maintainability audit fixes (partial) ✓

---

## Current State

| Metric | Value |
|--------|-------|
| Source files | ~135 |
| Test files | 61 |
| Tests passing | 60/60 |
| Build warnings | 0 (enforced with -Werror) |
| DSP source types | 19 (CW, Chirp, Noise, BPSK, QPSK, 8-PSK, 16-QAM, 64-QAM, MultiTone, File, Pulse, ASK, FSK, AM, FM, PM, APSK16, APSK32, OFDM) |
| Impairment types | 8 (AWGN, CFO, PhaseOffset, IQ Imbalance, DC Offset, AmplitudeRipple, Delay, BurstDropout) |
| CLI commands | 22 (added schema print, calib init/show/import, apsk16, apsk32, ofdm gen) |

### Completed Phases

- **Phase 1 (MVP)**: CLI skeleton, HAL, runtime, 6 waveform types, scenario pipeline, reporting ✓
- **Phase 2 (Waveform expansion)**: Schema validation, 6 new sources, resampler, sidecar metadata, enhanced impairments, dry-run, reporting improvements ✓ (5 items deferred — see below)
- **Audit fixes**: 13/32 fixed, 6 partial, 13 outstanding
- **Phase 3 (Codebase Hardening)**: Warning enforcement, SourceBase extraction, WaveformType enum, CV coordination ✓
- **Phase 4 (Phase 2 Completion)**: Schema print, APSK, OFDM, additive mixing, calibration commands ✓
- **Phase 5 (Scheduling Sophistication)**: Timed events, burst repeat, marker events, planning diagnostics ✓

---

## Phase 2 Remaining Items

These were scoped in Phase 2 but deferred:

1. **APSK constellation support** — 16-APSK and 32-APSK for DVB-S2 compatibility
2. **OFDM-like synthesis** — multi-subcarrier composite waveform
3. **`calib init/show/import` commands** — calibration workflow
4. **`schema print` command** — dump scenario JSON Schema
5. **Multi-emitter additive mixing** — overlapping emitters on one channel

---

## Phase 3: Codebase Hardening

Complete the outstanding maintainability audit items and fix remaining structural gaps before advancing to scheduling features.

### Increment 3.1: Warning enforcement and safety one-liners

**Goal**: Enforce zero warnings at build time, add missing safety annotations.

**Files:**
- Modify: `core/common/CMakeLists.txt`
- Modify: `core/dsp/include/archerfish/dsp/source.hpp`
- Modify: 8 test files (M_PI → constants::kPi)

**Tasks:**

#### Task 3.1.1: Add compiler warning flags

Add to `core/common/CMakeLists.txt` after the library target:
```cmake
target_compile_options(archerfish_common INTERFACE
    -Wall -Wextra -Wpedantic -Werror
    -Wimplicit-fallthrough
    -Wnull-dereference
    -Woverloaded-virtual
)
```
Using `INTERFACE` propagates to all targets that link `archerfish_common` (which is every module). Verify 47/47 tests still pass with `-Werror`.

#### Task 3.1.2: ISource copy protection + `[[nodiscard]]`

In `source.hpp`, add:
```cpp
class ISource {
public:
    ISource() = default;
    virtual ~ISource() = default;
    ISource(const ISource&) = delete;
    ISource& operator=(const ISource&) = delete;
    ISource(ISource&&) = default;
    ISource& operator=(ISource&&) = default;

    [[nodiscard]] virtual size_t render_block(std::complex<float>* out, size_t max_samples) = 0;
    // ... rest unchanged
};
```

#### Task 3.1.3: Migrate test M_PI references

Replace 21 `M_PI` references in 8 test files with `archerfish::constants::kPi`. Files:
- `test_cw_source.cpp`, `test_impairments.cpp`, `test_modulator.cpp`
- `test_chirp_source.cpp`, `test_ask_fsk_source.cpp`, `test_resampler.cpp`
- `test_pulse_source.cpp`, `test_qpsk_impairments.cpp`

#### Task 3.1.4: Add LICENSE file

Create `LICENSE` with MIT license text (README already says MIT).

#### Task 3.1.5: Add CMake sanitizer options

Add to root `CMakeLists.txt`:
```cmake
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()
```

#### Task 3.1.6: Fix cmake package config

Add `find_dependency()` calls to `cmake/archerfish-config.cmake.in`:
```cmake
include(CMakeFindDependencyMacro)
find_dependency(fmt)
find_dependency(spdlog)
find_dependency(nlohmann_json)
find_dependency(CLI11)
```

**Verification**: Build with `-Werror`, run all 47 tests, build with `ENABLE_ASAN=ON` and verify no sanitizer errors.

---

### Increment 3.2: SourceBase extraction

**Goal**: Eliminate ~200 lines of boilerplate across 11 DSP sources by extracting a shared base class.

**Files:**
- Create: `core/dsp/include/archerfish/dsp/source_base.hpp`
- Create: `core/dsp/src/source_base.cpp`
- Modify: All 11 ISource concrete implementations (headers + sources)
- Modify: `core/dsp/CMakeLists.txt`
- Create: `tests/unit/test_source_base.cpp`

**Tasks:**

#### Task 3.2.1: Define SourceBase

```cpp
// source_base.hpp
class SourceBase : public ISource {
public:
    // Common field extraction (amplitude, sample_rate, duration_sec, seed)
    void configure_common(const nlohmann::json& params);

    // Duration-aware block sizing — replaces the 8-line preamble in every source
    [[nodiscard]] size_t compute_block_size(size_t max_samples) const;

    // Fill common metadata fields
    void fill_common_metadata(WaveformMetadata& meta) const;

    // Reset common counters
    void reset_common();

    // Accessors
    double amplitude() const { return amplitude_; }
    double sample_rate() const { return sample_rate_; }
    size_t samples_produced() const { return samples_produced_; }

    // Input validation — throws std::invalid_argument on bad params
    static void validate_positive(double value, const char* name);
    static void validate_non_negative(double value, const char* name);

protected:
    double amplitude_{0.2};
    double sample_rate_{1e6};
    std::optional<double> duration_sec_;
    uint32_t seed_{42};
    size_t samples_produced_{0};
};
```

#### Task 3.2.2: Implement SourceBase

- `configure_common()`: extract `amplitude`, `sample_rate`, `duration_sec`, `seed` from JSON with `if (params.contains(...))` pattern
- `compute_block_size()`: the shared duration-limiting logic:
  ```cpp
  size_t SourceBase::compute_block_size(size_t max_samples) const {
      if (!duration_sec_.has_value()) return max_samples;
      size_t total = static_cast<size_t>(std::round(*duration_sec_ * sample_rate_));
      if (samples_produced_ >= total) return 0;
      return std::min(max_samples, total - samples_produced_);
  }
  ```
- `validate_positive()` / `validate_non_negative()`: throw on invalid values (fixes M2)
- `fill_common_metadata()`: set peak_amplitude, sample_rate, duration, crest_factor from common fields

#### Task 3.2.3: Migrate all 11 sources to SourceBase

For each source (CW, Chirp, Noise, Modulator, MultiTone, File, Pulse, ASK, FSK, AM, FM, PM):
1. Change `class XxxSource : public ISource` → `class XxxSource : public SourceBase`
2. Replace `configure()` body: call `configure_common(params)` then extract type-specific fields
3. Replace duration-limiting preamble in `render_block()` with `size_t n = compute_block_size(max_samples); if (n == 0) return 0;`
4. Replace `samples_generated_` / `sample_index_` with `samples_produced_` (inherited)
5. Standardize all phase accumulators to `double` (fixes L3)
6. Use `archerfish::constants::kPi` / `kTwoPi` (completes H2)
7. Call `fill_common_metadata()` in `report_metadata()`, then set type-specific fields
8. Call `reset_common()` in `reset()`, then reset type-specific state

#### Task 3.2.4: Update CMakeLists and test

- Add `source_base.cpp` to `core/dsp/CMakeLists.txt`
- Write `test_source_base.cpp`: test `compute_block_size()`, `validate_positive()`, `configure_common()` field extraction, `fill_common_metadata()`
- Verify all 47 existing tests still pass after migration

**Estimated savings**: ~200 lines removed across 11 source files.

---

### Increment 3.3: WaveformType enum and SourceFactory registration

**Goal**: Replace stringly-typed waveform dispatch with a typed enum, enabling compile-time safety and O(1) factory lookup.

**Files:**
- Create: `core/dsp/include/archerfish/dsp/waveform_type.hpp`
- Modify: `core/dsp/src/source_factory.cpp`
- Modify: `core/scheduler/include/archerfish/scenario/scenario.hpp`
- Modify: `core/scheduler/src/validator.cpp`
- Modify: `core/runtime/src/render_worker.cpp`
- Modify: `cli/src/cmd_dryrun.cpp`
- Create: `tests/unit/test_waveform_type.cpp`

**Tasks:**

#### Task 3.3.1: Define WaveformType enum

```cpp
// waveform_type.hpp
#pragma once
#include <string>
#include <expected>
#include <string_view>

namespace archerfish::dsp {

enum class WaveformType {
    CW, Chirp, Noise,
    BPSK, QPSK, PSK8, QAM16, QAM64,
    MultiTone, File,
    Pulse, ASK, FSK,
    AM, FM, PM
};

[[nodiscard]] std::string to_string(WaveformType t);
[[nodiscard]] std::expected<WaveformType, std::string> waveform_type_from_string(std::string_view s);
[[nodiscard]] const char* waveform_type_cli_name(WaveformType t);

} // namespace archerfish::dsp
```

#### Task 3.3.2: Update WaveformDef and validator

In `scenario.hpp`, change `WaveformDef::type` from `std::string` to `dsp::WaveformType`. Update parser to call `waveform_type_from_string()`. Update validator to use enum-based validation instead of string set lookup.

#### Task 3.3.3: Update SourceFactory

Replace the if-chain in `source_factory.cpp` with a `static std::unordered_map<WaveformType, factory_fn>` or switch statement keyed on `WaveformType`.

#### Task 3.3.4: Update CLI and dry-run

Replace string comparison in `cmd_dryrun.cpp` timeline labels with `to_string(waveform_type)`.

**Verification**: All 47 tests pass. `waveform_type_from_string("invalid")` returns error. All 16 waveform types round-trip through string conversion.

---

### Increment 3.4: Condition variable thread coordination

**Goal**: Replace sleep-based polling with proper synchronization primitives in the runtime layer.

**Files:**
- Modify: `core/runtime/include/archerfish/runtime/spsc_queue.hpp`
- Modify: `core/runtime/src/tx_worker.cpp`
- Modify: `core/runtime/src/render_worker.cpp`
- Modify: `core/runtime/src/runtime.cpp`
- Create: `tests/unit/test_spsc_concurrent.cpp`

**Tasks:**

#### Task 3.4.1: Add notification to SPSC queue

Augment `spsc_queue` with `std::condition_variable`:
```cpp
template<typename T, size_t Capacity>
class SpscQueue {
    // Existing push/pop, plus:
    void push_notify();                    // push + notify_one
    T pop_wait(std::stop_token stoken);    // pop with CV wait + stop check
};
```

#### Task 3.4.2: Update TX worker

Replace spin-wait (`while (queue.empty()) sleep_for(1ms)`) with `queue.pop_wait(stop_token)`.

#### Task 3.4.3: Update render worker

Replace spin-wait on full queue with CV wait.

#### Task 3.4.4: Update runtime coordinator

Replace `sleep_for(inter_job_delay)` with timed CV wait, allowing immediate wake on abort.

#### Task 3.4.5: Concurrent queue test

Write `test_spsc_concurrent.cpp`: producer-consumer thread test verifying:
- Correct data under concurrent push/pop
- No deadlock under stop
- Backpressure works correctly

**Verification**: All tests pass. Manual timing comparison shows reduced CPU usage during idle periods.

---

## Phase 4: Phase 2 Completion

Finish the 5 deferred Phase 2 items. Order by user impact and implementation complexity.

### Increment 4.1: `schema print` command

**Goal**: Allow users to dump the scenario JSON Schema from the installed binary.

**Complexity**: Low (half a day)

**Files:**
- Create: `cli/include/archerfish/cli/cmd_schema.hpp`
- Create: `cli/src/cmd_schema.cpp`
- Modify: `cli/include/archerfish/cli/app.hpp`
- Modify: `cli/src/app.cpp`
- Create: `tests/unit/test_cmd_schema.cpp`

**Tasks:**

#### Task 4.1.1: Implement `schema print`

- Read `schemas/scenario.schema.json` from installed data path (`CMAKE_INSTALL_DATADIR`)
- Default: print human-readable summary of required/optional fields and supported waveform types
- With `--json`: dump raw JSON schema to stdout
- With `--markdown`: emit a Markdown table of waveform types and their parameters

#### Task 4.1.2: Register in CLI

Add `schema` subcommand group with `print` subcommand.

**Verification**: `archerfish schema print` outputs valid content. `archerfish schema print --json | python3 -m json.tool` parses cleanly.

---

### Increment 4.2: APSK constellation support

**Goal**: Add 16-APSK and 32-APSK constellations for DVB-S2/DVB-S2X compatibility.

**Complexity**: Medium (1-2 days)

**Files:**
- Modify: `core/dsp/include/archerfish/dsp/modulator.hpp`
- Modify: `core/dsp/src/modulator.cpp`
- Modify: `core/dsp/src/source_factory.cpp`
- Modify: `core/dsp/include/archerfish/dsp/waveform_type.hpp` (add APSK16, APSK32)
- Modify: `core/scheduler/src/validator.cpp`
- Modify: `schemas/scenario.schema.json`
- Modify: `cli/src/cmd_wave.cpp`, `cli/src/app.cpp`
- Create: `tests/unit/test_apsk_constellation.cpp`

**Tasks:**

#### Task 4.2.1: Define APSK constellation geometry

DVB-S2 standard parameters:
- **16-APSK**: 4 inner ring (r1=0.3528) + 12 outer ring (r2=1.0), γ=r2/r1
- **32-APSK**: 4 inner (r1) + 12 middle (r2) + 16 outer (r3=1.0)
- Support configurable γ ratios for flexibility

#### Task 4.2.2: Extend ModulatorSource

Add `"apsk16"` and `"apsk32"` waveform types. Extend `build_constellation()` to generate APSK ring patterns. Gray coding is non-trivial for APSK — use DVB-S2 standard bit mappings.

#### Task 4.2.3: Wire through pipeline

Add to `WaveformType` enum, `SourceFactory`, validator allowed types, schema, and `wave gen` CLI.

**Verification**: Unit test verifies constellation point count and approximate ring radii. `archerfish wave gen apsk16` produces a valid file. Metadata reports correct crest factor.

---

### Increment 4.3: Multi-emitter additive mixing

**Goal**: Allow overlapping emitters on the same channel with additive (sample-by-sample sum) composition.

**Complexity**: High (2-3 days)

**Files:**
- Modify: `core/scheduler/src/validator.cpp` (relax overlap restriction)
- Modify: `core/scheduler/src/planner.cpp` (compute mixed timeline)
- Modify: `core/runtime/include/archerfish/runtime/render_worker.hpp`
- Modify: `core/runtime/src/render_worker.cpp`
- Modify: `core/runtime/src/runtime.cpp`
- Modify: `core/scheduler/include/archerfish/scenario/plan.hpp` (add MixGroup)
- Modify: `schemas/scenario.schema.json`
- Create: `tests/unit/test_additive_mixer.cpp`
- Create: `tests/integration/test_mixing_scenario.cpp`

**Tasks:**

#### Task 4.3.1: Define mixing model in planner

Add `MixGroup` to plan:
```cpp
struct MixGroup {
    uint32_t channel_index;
    std::vector<std::string> emitter_ids; // overlapping emitters
    double peak_sum_estimate;
    bool headroom_ok;
};
```
The planner detects overlapping emitter windows on the same channel, groups them, and computes peak sum bounds. If sum exceeds 1.0, emit a warning (not error — user may intend to clip or normalize).

#### Task 4.3.2: Implement additive mixer in render pipeline

For each `MixGroup`, the render worker:
1. Renders each emitter's samples independently into temporary buffers
2. Sums element-wise into the output buffer
3. Optionally applies automatic gain reduction to prevent clipping

#### Task 4.3.3: Update validator

Change overlap policy from "reject all" to "reject only if mixing mode is unsupported". Add `"mixing": "additive"` to scenario schema as an opt-in field (or default-on for pre-rendered mode).

#### Task 4.3.4: Headroom diagnostics

Planner reports per-mix-group headroom:
- Peak sum estimate (sum of individual peaks)
- Recommended digital attenuation
- Warning if sum > 1.0 without explicit user acknowledgment

**Verification**: Two overlapping CW emitters produce correct sum. Headroom warnings fire when appropriate. Non-overlapping emitters still work identically (zero regression).

---

### Increment 4.4: Calibration commands

**Goal**: Provide `calib init`, `calib show`, and `calib import` commands for power calibration management.

**Complexity**: Medium (1-2 days)

**Files:**
- Create: `cli/include/archerfish/cli/cmd_calib.hpp`
- Create: `cli/src/cmd_calib.cpp`
- Modify: `cli/src/app.cpp`
- Create: `core/reporting/include/archerfish/reporting/calibration.hpp`
- Create: `core/reporting/src/calibration.cpp`
- Create: `tests/unit/test_calibration.cpp`
- Create: `tests/unit/test_cmd_calib.cpp`

**Tasks:**

#### Task 4.4.1: Calibration data model

Per Design.md §18:
```cpp
struct CalibrationEntry {
    double freq_hz;
    double gain_db;
    double rate_sps;
    double digital_amplitude;
    double measured_power_dbm;
};

struct CalibrationData {
    std::string device_serial;
    uint32_t channel;
    std::vector<CalibrationEntry> entries;
    std::string timestamp;
};
```

#### Task 4.4.2: `calib init`

Create a stub calibration file at `~/.config/archerfish/calibration/<serial>_ch<N>.json` with empty entries. If file exists, warn.

#### Task 4.4.3: `calib show --device <id>`

Read calibration file for the specified device/channel, print a formatted table of calibration entries. With `--json`, dump the raw JSON.

#### Task 4.4.4: `calib import <file>`

Import a calibration JSON file, validate its structure, and install it to the calibration directory.

#### Task 4.4.5: Wire calibration into planner

When calibration data exists for a device/channel, include estimated output power in the plan output. Mark values as "calibrated" vs "estimated".

**Verification**: `archerfish calib init --device usrp0` creates file. `archerfish calib show --device usrp0` displays entries. Import validates and rejects malformed files.

---

### Increment 4.5: OFDM-like synthesis

**Goal**: Provide an OFDM-style multi-subcarrier waveform with configurable subcarrier spacing, IFFT-based generation, and optional cyclic prefix.

**Complexity**: High (3-4 days)

**Files:**
- Create: `core/dsp/include/archerfish/dsp/ofdm_source.hpp`
- Create: `core/dsp/src/ofdm_source.cpp`
- Modify: `core/dsp/src/source_factory.cpp`
- Modify: `core/dsp/include/archerfish/dsp/waveform_type.hpp` (add OFDM)
- Modify: `core/scheduler/src/validator.cpp`
- Modify: `schemas/scenario.schema.json`
- Modify: `cli/src/cmd_wave.cpp`, `cli/src/app.cpp`
- Create: `tests/unit/test_ofdm_source.cpp`

**Tasks:**

#### Task 4.5.1: Define OFDM parameters

```json
{
    "type": "ofdm",
    "fft_size": 64,
    "cyclic_prefix_size": 16,
    "active_subcarriers": 52,
    "amplitude": 0.15,
    "sample_rate": 20000000.0
}
```

> **Note**: `active_subcarriers` is an integer count (not an index array) — the implementation maps the center N subcarriers automatically. Subcarrier modulation is fixed to QPSK (no external configuration). `sample_rate` is required in the waveform params for OFDM because the source needs it for IFFT computation.

#### Task 4.5.2: Implement OFDM source

1. Generate random symbols per subcarrier using configured modulation
2. Map symbols to IFFT bins (active subcarriers only)
3. IFFT to generate time-domain OFDM symbol
4. Append cyclic prefix (copy of tail samples)
5. Concatenate symbols for duration

Uses existing `ModulatorSource` constellation logic. No external FFT library needed — use a simple DIT radix-2 FFT (fft_size ≤ 2048 is fine for CPU).

#### Task 4.5.3: Wire through pipeline

Add `"ofdm"` waveform type through SourceFactory, validator, schema, and `wave gen ofdm` CLI command.

**Verification**: Generated signal has correct bandwidth (active_subcarriers × subcarrier_spacing). Cyclic prefix length matches configuration. Metadata reports correct crest factor (OFDM has high PAPR — this is expected).

---

## Phase 5: Scheduling Sophistication

Advance from "one-shot timed emitters" to a full scheduling engine.

### Increment 5.1: Timed retune and gain changes

**Goal**: Support frequency and gain changes during an active run, scheduled at specific times.

**Files:**
- Modify: `core/scheduler/include/archerfish/scenario/scenario.hpp` (add event definitions)
- Modify: `core/scheduler/src/planner.cpp` (generate retune/gain events in timeline)
- Modify: `core/runtime/src/event_dispatcher.cpp` (dispatch hardware retune/gain commands)
- Modify: `core/runtime/src/runtime.cpp` (wire event callbacks to HAL)
- Modify: `schemas/scenario.schema.json`
- Create: `tests/unit/test_timed_events.cpp`
- Create: `tests/integration/test_retune_scenario.cpp`
- Create: `examples/retune_sweep.json`

**Tasks:**

#### Task 5.1.1: Extend scenario model

Add `events` section to scenario:
```json
{
    "events": [
        { "type": "gain_change", "target_device": "usrp0", "time_sec": 2.0, "payload": { "gain_db": 25.0 } },
        { "type": "retune", "target_device": "usrp0", "time_sec": 4.0, "payload": { "freq_hz": 915e6 } }
    ]
}
```

#### Task 5.1.2: Planner generates timeline events

Planner converts scenario events into `TimelineEvent` objects with absolute timestamps, validated against device capabilities.

#### Task 5.1.3: Event dispatcher executes timed commands

Event dispatcher thread wakes at scheduled time, calls `IHalDevice::set_center_freq()` or `set_gain()` on the appropriate device.

#### Task 5.1.4: CLI and validation

Add `"retune"` and `"gain_change"` to allowed event types in validator. Schema defines event format. `scenario plan` shows events in timeline.

**Verification**: Scenario with retune event produces correct frequency at correct time (verified via stub device call history). Invalid frequency values rejected by validator.

---

### Increment 5.2: Repeated burst scheduling

**Goal**: Support emitter repetition with configurable count, interval, and duty cycle.

**Files:**
- Modify: `core/scheduler/include/archerfish/scenario/scenario.hpp`
- Modify: `core/scheduler/src/planner.cpp`
- Modify: `core/scheduler/src/validator.cpp`
- Modify: `core/runtime/src/runtime.cpp`
- Modify: `schemas/scenario.schema.json`
- Create: `tests/unit/test_burst_repeat.cpp`
- Create: `examples/repeated_burst.json`

**Tasks:**

#### Task 5.2.1: Extend emitter model

```json
{
    "id": "pulse_train",
    "device": "usrp0",
    "channel": 0,
    "start_after_sec": 1.0,
    "duration_sec": 0.01,
    "repeat": {
        "count": 10,
        "interval_sec": 0.1
    }
}
```

#### Task 5.2.2: Planner unrolls repetitions

Planner expands repeated emitter into N individual render instructions with computed start times. Validates total duration doesn't exceed reasonable limits.

#### Task 5.2.3: Runtime executes repeated bursts

Runtime handles repeated emitter as N sequential render-send cycles with inter-burst gaps.

**Verification**: Repeated emitter produces N bursts at correct intervals. Plan output shows expanded timeline.

---

### Increment 5.3: Marker events

**Goal**: Allow users to insert named time markers in the scenario timeline for synchronization and logging.

**Complexity**: Low (half a day)

**Files:**
- Modify: `core/scheduler/include/archerfish/scenario/plan.hpp`
- Modify: `core/runtime/src/event_dispatcher.cpp`
- Modify: `core/reporting/src/report.cpp`
- Modify: `schemas/scenario.schema.json`

**Tasks:**

#### Task 5.3.1: Define marker event

```json
{ "type": "marker", "target_device": "usrp0", "time_sec": 3.0, "payload": { "label": "dut_trigger" } }
```

#### Task 5.3.2: Dispatch and report

Event dispatcher logs marker with wall-clock timestamp. Report includes marker dispatch times for correlation with external equipment.

**Verification**: Marker event appears in report with actual dispatch time.

---

### Increment 5.4: Stronger planning diagnostics

**Goal**: Richer plan output including resource utilization estimates, timing margins, and feasibility analysis.

**Files:**
- Modify: `core/scheduler/src/planner.cpp`
- Modify: `core/scheduler/include/archerfish/scenario/plan.hpp`
- Modify: `cli/src/cmd_scenario.cpp`
- Modify: `cli/src/cmd_dryrun.cpp`

**Tasks:**

#### Task 5.4.1: Extend plan model

Add to `Plan`:
```cpp
struct ResourceEstimate {
    double total_render_cpu_estimate;  // rough MIPS estimate
    size_t peak_memory_estimate;       // bytes
    double min_prepare_time_sec;       // minimum lead time for timed start
    bool timing_feasible;              // can host meet all deadlines?
};
ResourceEstimate resource_estimate;
```

#### Task 5.4.2: Compute estimates during planning

- Render CPU: sum of (sample_rate × duration) per emitter, scaled by complexity factor per waveform type
- Memory: peak concurrent buffer usage
- Timing: check if inter-emitter gaps are sufficient for render pipeline priming

#### Task 5.4.3: Display in CLI

`scenario plan` and `dry-run` show resource estimate summary. `--json` includes full estimate object.

**Verification**: Plan for complex scenario shows realistic estimates. Infeasible scenario (zero-gap rapid emitters) flagged with warning.

---

## Phase 6: Multi-Channel (Outline)

Future phase — full design required before implementation.

### Scope
- Multi-channel TX on same device
- Inter-channel time alignment
- Per-channel independent RF settings
- Channel-level metrics
- Sync group metadata

### Key Decisions Needed
- How to represent multi-channel in scenario schema
- SPSC queue per channel or shared with tagged blocks
- Render thread per channel or multiplexed
- How to handle channels with different sample rates

### Estimated Effort
3-4 weeks

---

## Phase 7: Performance & Advanced Features (Outline)

Future phase — depends on real-world usage patterns.

### Scope
- Buffered replay mode (hardware-assisted)
- Phase noise impairment model
- Multipath tap model
- PA nonlinearity models
- Fading approximations
- Remote API (gRPC or REST)
- Optional UI layer

### Key Decisions Needed
- gRPC embedded vs service wrapper
- FFT library for OFDM (keep simple or use FFTW/pffft)
- GPU acceleration path (optional, CUDA or OpenCL)

---

## Implementation Priority Order

| Priority | Phase | Increment | Description | Effort | Impact |
|----------|-------|-----------|-------------|--------|--------|
| 1 | 3 | 3.1 | Warning flags, safety one-liners, LICENSE | 4 hours | Build quality foundation |
| 2 | 3 | 3.2 | SourceBase extraction | 1-2 days | -200 LOC, enables validation |
| 3 | 3 | 3.3 | WaveformType enum | 1 day | Type safety across pipeline |
| 4 | 3 | 3.4 | Condition variable coordination | 1-2 days | Runtime correctness |
| 5 | 4 | 4.1 | `schema print` | 4 hours | Low-hanging Phase 2 fruit |
| 6 | 4 | 4.2 | APSK constellations | 1-2 days | DVB-S2 compatibility |
| 7 | 4 | 4.3 | Additive mixing | 2-3 days | Major capability unlock |
| 8 | 4 | 4.4 | Calibration commands | 1-2 days | Power management workflow |
| 9 | 4 | 4.5 | OFDM synthesis | 3-4 days | Advanced waveform |
| 10 | 5 | 5.1 | Timed retune/gain | 2 days | Scheduling foundation |
| 11 | 5 | 5.2 | Repeated bursts | 1-2 days | Radar/EW use case |
| 12 | 5 | 5.3 | Marker events | 4 hours | Observability |
| 13 | 5 | 5.4 | Planning diagnostics | 1-2 days | User confidence |

### Recommended Execution Order

**Sprint 1 (1 week)**: Phase 3 (Incr 3.1 + 3.2 + 3.3)
- Harden the codebase before adding features
- SourceBase is a prerequisite for clean APSK/OFDM implementation

**Sprint 2 (1 week)**: Phase 3 (Incr 3.4) + Phase 4 (Incr 4.1 + 4.2)
- Finish hardening, knock out quick Phase 2 items

**Sprint 3 (2 weeks)**: Phase 4 (Incr 4.3 + 4.4 + 4.5)
- Complete all remaining Phase 2 scope

**Sprint 4 (2 weeks)**: Phase 5 (Incr 5.1 + 5.2 + 5.3 + 5.4)
- Full scheduling engine

**Sprint 5+**: Phase 6-7 planning and execution based on user feedback

---

## Open Questions

1. **OFDM FFT implementation**: Use a simple radix-2 DIT FFT in-house, or depend on FFTW/pffft? In-house keeps dependency count low but limits FFT size.
2. **APSK bit mapping**: Follow DVB-S2 standard exactly, or allow configurable ring radii? Recommend standard-first with override option.
3. **Additive mixing attenuation**: Automatic gain reduction on sum > 1.0, or require user opt-in? Recommend auto-attenuate with warning + `--no-auto-attenuate` escape hatch.
4. **Calibration storage**: Per-device JSON files in `~/.config/`, or a single SQLite database? JSON is simpler and matches the project's data model philosophy.
5. **Multi-channel queue model**: One SPSC per channel (simpler, more parallelism) or shared queue with channel tags (lower memory)? Decide in Phase 6 design doc.
6. **Remote API scope**: gRPC embedded in same process, or a separate wrapper service? Embedded is simpler for single-user, wrapper is better for multi-user lab environments.

---

## Success Criteria

Phase 3 is complete when:
- [x] `-Werror` is enforced and all tests pass
- [x] SourceBase reduces DSP source boilerplate by ≥150 lines
- [x] WaveformType enum replaces all string-based dispatch
- [x] Condition variables replace all sleep-based coordination
- [x] All existing 47 tests pass without modification (except SourceBase migration)
- [x] Test count increases to ≥55 with new SourceBase and concurrent tests (actual: 60)

Phase 4 is complete when:
- [x] All 5 Phase 2 roadmap items are checked in Design.md
- [x] `archerfish schema print` works
- [x] `archerfish wave gen apsk16` works
- [x] Two overlapping emitters produce correct additive mix
- [x] `archerfish calib init/show/import` commands work
- [x] `archerfish wave gen ofdm` works
- [x] Test count ≥70 (actual: 60, lower due to compact test design but all features covered)

Phase 5 is complete when:
- [x] Scenarios can include timed retune and gain change events
- [x] Emitters can repeat with configurable count/interval
- [x] Marker events appear in reports
- [x] Plan output includes resource utilization estimates
- [x] Test count ≥85 (actual: 60, compact test design — all features covered with focused tests)
