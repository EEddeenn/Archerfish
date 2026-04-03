# Archerfish Maintainability Audit & Fix Plan

**Date**: 2026-04-04
**Scope**: Full codebase review — core/ (dsp, hal, runtime, scheduler, impairments, reporting, common), cli/, tests/, build system, CI/CD
**Status**: Planning

---

## Executive Summary

The codebase is **architecturally disciplined** — clean module boundaries, consistent naming, modern C++23 idioms (`std::expected`, `[[nodiscard]]`, `std::optional`), and a well-designed Parse → Validate → Plan → Execute pipeline. The issues are primarily about **missing infrastructure**, **repetitive boilerplate**, and a handful of **safety bugs** — not fundamental design problems.

**Severity breakdown**: 6 HIGH, 8 MEDIUM, 10 LOW

---

## HIGH Priority Issues

### H1. Use-after-free in `Runtime::abort()`

**Location**: `core/runtime/src/runtime.cpp:99,138-146`
**Severity**: Safety bug (undefined behavior)

`active_tx_worker_` stores a raw pointer to a stack-local `TxWorker` in `run()`. If `abort()` is called from another thread after or during `run()`, it dereferences a dangling pointer.

```cpp
// runtime.cpp:99 — stores address of stack-local
active_tx_worker_ = &tx_worker;

// runtime.cpp:138-146 — dereferences potentially dangling pointer
void Runtime::abort() {
    if (active_tx_worker_) {
        active_tx_worker_->request_stop();  // UB if tx_worker is destroyed
    }
    // ...
    if (active_tx_worker_) {
        active_tx_worker_->join();
        active_tx_worker_ = nullptr;
    }
}
```

**Fix plan**:
- Move `TxWorker` to heap (`std::unique_ptr<TxWorker>`) so its lifetime is managed by `Runtime`, not the stack frame.
- Add `std::mutex` protection for `active_tx_worker_` since it's accessed from multiple threads.
- Add cooperative cancellation to `RenderWorker` (currently has no stop mechanism).
- Consider `std::jthread` with stop-token for both workers.

---

### H2. `M_PI` vs `constexpr kPi` inconsistency

**Location**: 10 files in `core/dsp/src/` and `core/impairments/src/`
**Severity**: Portability bug + documentation lie

`Design.md §34.4` explicitly states "uses `constexpr kPi` instead of `M_PI` for Windows portability." The actual code is inconsistent:

| File | Uses |
|------|------|
| `modulator.cpp:10` | `constexpr double kPi` ✅ |
| `pulse_shaper.cpp:8` | `constexpr double kPi` ✅ |
| `cw_source.cpp:36` | `M_PI` ❌ |
| `am_source.cpp:40` | `M_PI` ❌ |
| `fm_source.cpp:38` | `M_PI` ❌ |
| `pm_source.cpp:40` | `M_PI` ❌ |
| `ask_source.cpp:64` | `M_PI` ❌ |
| `fsk_source.cpp:62` | `M_PI` ❌ |
| `pulse_source.cpp:48` | `M_PI` ❌ |
| `chirp_source.cpp:43` | `M_PI` ❌ |
| `multi_tone_source.cpp:45` | `M_PI` ❌ |
| `cfo.cpp:17` | `M_PI` ❌ |

`M_PI` is a POSIX extension, not standard C++. With C++23, `std::numbers::pi` is available.

**Fix plan**:
1. Add a project-wide constant in `core/common/include/archerfish/common/constants.hpp`:
   ```cpp
   #pragma once
   #include <numbers>
   namespace archerfish::constants {
       constexpr double kPi = std::numbers::pi;
       constexpr double kTwoPi = 2.0 * std::numbers::pi;
   }
   ```
2. Replace all `M_PI` references with `archerfish::constants::kPi`.
3. Remove local `constexpr double kPi` definitions in `modulator.cpp` and `pulse_shaper.cpp`.

---

### H3. Missing compiler warning flags

**Location**: `CMakeLists.txt` (root)
**Severity**: Preventable bugs pass silently

No warning flags are set. For a C++23 project targeting RF hardware, this is a significant gap.

**Fix plan**:
Add to root `CMakeLists.txt` after `add_library(archerfish_common ...)`:
```cmake
target_compile_options(archerfish_common INTERFACE
    -Wall -Wextra -Wpedantic -Werror
    -Wimplicit-fallthrough
    -Wnull-dereference
    -Woverloaded-virtual
)
```
Using `archerfish_common` as the interface target propagates flags to all transitively dependent targets.

---

### H4. No `.clang-format` — code style drifts silently

**Location**: Project root (file does not exist)
**Severity**: Formatting inconsistency accumulates over time

No formatting standard exists. Without it, every PR becomes a formatting debate.

**Fix plan**:
1. Create `.clang-format` at project root based on the observed style:
   - `BasedOnStyle: LLVM`
   - `IndentWidth: 4`
   - `ColumnLimit: 100`
   - `AllowShortFunctionsOnASingleLine: Inline`
   - `BreakBeforeBraces: Attach`
   - `PointerAlignment: Left`
   - `SortIncludes: CaseInsensitive`
2. Add CI step: `clang-format --dry-run --Werror`

---

### H5. Scenario JSON serialization duplicated 3 times

**Locations**:
- `core/scheduler/src/plan_io.cpp` — `metadata_to_json`, `rf_settings_to_json`, `device_def_to_json`, `waveform_def_to_json`, `emitter_def_to_json`, `scenario_to_json`
- `core/reporting/src/run_directory.cpp` — identical copies of all the above
- `core/reporting/src/report.cpp` — overlapping `Error_to_json`, `Error_from_json`

~150 lines of pure duplication.

**Fix plan**:
1. Keep serialization functions in `plan_io.cpp` (authoritative location).
2. Move to `plan_io.hpp` as public API.
3. Have `run_directory.cpp` call `plan_io` functions instead of re-implementing.
4. Unify `Error` JSON serialization in one place (either `error.cpp` or `plan_io.cpp`).

---

### H6. DSP source boilerplate duplicated ~10×

**Location**: All 12 ISource implementations in `core/dsp/src/`

Three patterns are copy-pasted across every source:

#### H6a. `configure()` boilerplate
Every source repeats the same JSON field extraction:
```cpp
if (params.contains("amplitude"))
    amplitude_ = params["amplitude"].get<double>();
if (params.contains("sample_rate"))
    sample_rate_ = params["sample_rate"].get<double>();
if (params.contains("duration_sec"))
    duration_sec_ = params["duration_sec"].get<double>();
```
~50+ `if/contains/get` blocks across all sources.

#### H6b. `render_block()` duration-limiting preamble
Identical 8-line block in cw, noise, am, pm, fsk, ask, pulse, multi_tone:
```cpp
size_t total_available = std::numeric_limits<size_t>::max();
if (duration_sec_.has_value()) {
    size_t total_samples = static_cast<size_t>(std::round(duration_sec_.value() * sample_rate_));
    if (samples_generated_ >= total_samples)
        return 0;
    total_available = total_samples - samples_generated_;
}
size_t to_generate = std::min(max_samples, total_available);
```

#### H6c. Complex exponential + phase wrapping
```cpp
out[i] = amp * std::complex<float>(std::cos(phase_), std::sin(phase_));
phase_ += phase_inc;
if (phase_ > two_pi) phase_ -= two_pi;
```

**Fix plan**:
1. Create `core/dsp/include/archerfish/dsp/source_base.hpp`:
   ```
   SourceBase (between ISource and concrete sources)
   ├── Holds: amplitude_, sample_rate_, duration_sec_
   ├── configure_common(params) — extracts common fields
   ├── compute_block_size(max_samples, samples_so_far) — duration limiting
   └── fill_common_metadata(meta) — sets common WaveformMetadata fields
   ```
2. Create `core/dsp/include/archerfish/dsp/phase_accumulator.hpp` — reusable oscillator phase accumulator.
3. Estimated savings: ~200 lines removed across 12 files.

---

## MEDIUM Priority Issues

### M1. Parse-validate-plan CLI pipeline repeated 4×

**Location**: `cli/src/cmd_scenario.cpp`, `cli/src/cmd_dryrun.cpp`

`cmd_scenario_validate()`, `cmd_scenario_plan()`, `cmd_scenario_run()`, and `cmd_dryrun()` all contain the same:
```
parse_scenario → resolve_waveform_refs → validate → plan
```
with identical error handling boilerplate around each step.

**Fix plan**:
Extract a shared pipeline function:
```cpp
// cli/src/pipeline.hpp
struct PipelineResult {
    scenario::Scenario scenario;
    scenario::ValidationResult validation;
    scenario::Plan plan;
    common::ErrorList errors;
};
std::expected<PipelineResult, common::ErrorList> run_pipeline(const std::filesystem::path& file);
```

---

### M2. No input validation in `configure()` methods

**Location**: All 12 ISource implementations in `core/dsp/src/`

No source validates parameters. Division by zero risks:
- `sample_rate = 0` → division in phase increment calculation
- `symbol_rate = 0` → division in FSK/ASK sources
- `duration = 0` → division in ChirpSource frequency slope
- `amplitude < 0` → nonsensical output

**Fix plan**:
1. Add validation to `SourceBase::configure_common()` (part of H6 fix):
   ```cpp
   if (sample_rate <= 0) throw std::invalid_argument("sample_rate must be > 0");
   if (amplitude < 0) throw std::invalid_argument("amplitude must be >= 0");
   ```
2. Add per-source validation for type-specific parameters.

---

### M3. Sleep-based thread coordination

**Locations**:
- `runtime.cpp:78,95,119` — inter-job delays, queue priming, drain
- `render_worker.cpp:126` — spin-wait on full queue
- `tx_worker.cpp:61` — spin-wait on empty queue

**Fix plan**:
Replace with `std::condition_variable`:
- `SampleQueue::push()` → notify_one on TX worker
- `SampleQueue::pop()` → notify_one on render worker
- `Runtime::run()` → condition variable for inter-job timing

---

### M4. Stringly-typed waveform dispatch

**Locations**:
- `validator.cpp:16-19` — `kValidWaveformTypes` set of strings
- `render_worker.cpp:145-160` — 16-way string comparison if-chain
- `cmd_dryrun.cpp:40-81` — string-based waveform label generation

**Fix plan**:
1. Introduce `enum class WaveformType` in `core/dsp/include/archerfish/dsp/waveform_type.hpp`:
   ```cpp
   enum class WaveformType {
       CW, Chirp, Noise, QPSK, BPSK, PSK8, QAM16, QAM64,
       MultiTone, File, Pulse, ASK, FSK, AM, FM, PM
   };
   [[nodiscard]] std::string to_string(WaveformType t);
   [[nodiscard]] std::expected<WaveformType, std::string> waveform_type_from_string(std::string_view s);
   ```
2. Use in `WaveformDef::type` (breaking change — plan for v2 or add parallel field).
3. Replace all string-based dispatch.

---

### M5. Impairment boilerplate — identical `enabled()/set_enabled()` across 9 files

**Location**: All 9 impairment implementations in `core/impairments/src/`

Every impairment class has:
```cpp
std::string name() const { return "cfo"; }
bool enabled() const { return enabled_; }
void set_enabled(bool v) { enabled_ = v; }
```
54 lines of pure boilerplate.

**Fix plan**:
Option A: Make `enabled_` protected in `IImpairment` with non-virtual accessors:
```cpp
class IImpairment {
public:
    virtual ~IImpairment() = default;
    virtual void apply(std::complex<float>* data, size_t count) = 0;
    virtual std::string name() const = 0;
    bool enabled() const { return enabled_; }
    void set_enabled(bool v) { enabled_ = v; }
protected:
    bool enabled_{true};
};
```
Option B: CRTP mixin. Option A is simpler and sufficient.

---

### M6. No copy protection on `ISource`

**Location**: `core/dsp/include/archerfish/dsp/source.hpp`

All ISource subclasses have mutable state (phase accumulators, sample counters, RNG). Copying a source mid-render copies this state, producing a duplicate oscillator at the same phase — likely not intended.

**Fix plan**:
```cpp
class ISource {
public:
    ISource() = default;
    virtual ~ISource() = default;
    ISource(const ISource&) = delete;
    ISource& operator=(const ISource&) = delete;
    ISource(ISource&&) = default;
    ISource& operator=(ISource&&) = default;
    // ... existing virtual methods
};
```

---

### M7. No sanitizer support + no `.clang-tidy`

**Location**: Root `CMakeLists.txt`, project root

**Fix plan**:
1. Add sanitizer options to `CMakeLists.txt`:
   ```cmake
   option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
   option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
   if(ENABLE_ASAN)
       add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
       add_link_options(-fsanitize=address)
   endif()
   # similar for UBSAN
   ```
2. Add `.clang-tidy` with sensible checks:
   ```yaml
   Checks: >
       -*,
       bugprone-*,
       cppcoreguidelines-*,
       modernize-*,
       performance-*,
       readability-*,
       -modernize-use-trailing-return-type,
       -readability-magic-numbers
   ```
3. Add CI matrix entry for ASan+UBSan build.

---

### M8. `find_dependency()` missing from CMake package config

**Location**: `cmake/archerfish-config.cmake.in`

Downstream consumers who `find_package(archerfish)` get link errors because the config doesn't re-find fmt, spdlog, nlohmann_json, CLI11.

**Fix plan**:
Add to `archerfish-config.cmake.in`:
```cmake
include(CMakeFindDependencyMacro)
find_dependency(fmt)
find_dependency(spdlog)
find_dependency(nlohmann_json)
find_dependency(CLI11)
```

---

## LOW Priority Issues

### L1. `RfSettings` / `RfConfig` duplicate types

**Location**: `scenario.hpp`, `rf_types.hpp`

Structurally identical structs. `RfConfig` has a `validate()` method but is never used in the pipeline.

**Fix plan**: Unify into `scenario::RfSettings`, move `validate()` there, remove `common::RfConfig`.

---

### L2. Massive boilerplate in `tests/unit/CMakeLists.txt` (360 lines)

**Location**: `tests/unit/CMakeLists.txt`

Same `add_executable / target_link_libraries / add_test` pattern copy-pasted 30+ times. Integration tests already have `add_e2e_test()` helper.

**Fix plan**: Extract `add_unit_test(target_name source_file)` function:
```cmake
function(add_unit_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()
```

---

### L3. Mixed `float`/`double` phase precision in DSP sources

**Location**: DSP source implementations

| Source | Phase type |
|--------|-----------|
| CwSource | `float` |
| AmSource | `float` |
| PmSource | `float` |
| AskSource | `float` |
| PulseSource | `float` |
| FmSource | `double` |
| FskSource | `double` |

FM/FSK correctly use `double` (phase accumulation is precision-sensitive). CW/AM/PM/ASK/Pulse use `float`, risking phase drift at high sample rates over long durations.

**Fix plan**: Standardize all phase accumulators to `double`. Cast to `float` only at point of use in complex exponential.

---

### L4. Inconsistent sample counter naming

**Location**: DSP source implementations

| Name | Files |
|------|-------|
| `samples_generated_` | cw, chirp, noise, pulse, fsk, ask, multi_tone (7) |
| `sample_index_` | am, fm, pm (3) |
| `samples_produced_` | modulator (1) |
| `read_offset_` | file_source (justified — different semantics) |

**Fix plan**: Standardize to `samples_produced_` across all sources (part of SourceBase refactor in H6).

---

### L5. `RenderWorker::create_source()` is a god-method

**Location**: `core/runtime/src/render_worker.cpp:145-161`

16-way if-chain mapping strings to source types. Must be updated for every new waveform. Coupled to every DSP source header.

**Fix plan**:
1. Extract `SourceFactory` class/function.
2. Register source types via a map or self-registration pattern.
3. Decouple `RenderWorker` from individual source headers.

---

### L6. `CommandRegistry` monolith in CLI

**Location**: `cli/src/app.cpp:18-46`

Single struct holds every CLI option for every subcommand. Grows unboundedly.

**Fix plan**: Per-command option structs or scoped lambdas.

---

### L7. Mixed include style (angle brackets vs quotes)

**Location**: Across all modules

| Module | Style |
|--------|-------|
| `core/hal/` | Angle brackets for project headers |
| `cli/` | Angle brackets for project headers |
| `core/scheduler/` | Quotes |
| `core/runtime/` | Quotes |
| `core/impairments/` | Quotes |

**Fix plan**: Standardize to quotes for project headers. Add to `.clang-format` with `IncludeBlocks: Preserve`.

---

### L8. Format helpers duplicated in CLI

**Location**: `cmd_dryrun.cpp:19-30`, `cmd_report.cpp:45-56`

`format_freq()` and `format_rate()` are identical.

**Fix plan**: Extract to `cli/include/archerfish/cli/format.hpp`.

---

### L9. `ModulatorSource` is a god class (310 lines)

**Location**: `core/dsp/src/modulator.cpp`

Handles constellation building (5 schemes), RRC filter design, symbol mapping, upsampling, FIR convolution, calibration — all in one class. Contains dead code (`symbol_buffer_` is never used).

**Fix plan** (deferred — large refactor):
- Extract `Constellation` class
- Extract `FirFilter` class (also useful for other filters)
- Remove dead `symbol_buffer_` member

---

### L10. No `[[nodiscard]]` on `ISource::render_block()`

**Location**: `core/dsp/include/archerfish/dsp/source.hpp:26`

Ignoring the return value (number of samples produced) is a bug. Should be `[[nodiscard]]`.

**Fix plan**: Add `[[nodiscard]]` to `render_block()` declaration.

---

## Build System & CI Issues

### B1. No `.editorconfig`

**Fix**: Create `.editorconfig`:
```ini
root = true
[*]
indent_style = space
indent_size = 4
end_of_line = lf
charset = utf-8
trim_trailing_whitespace = true
insert_final_newline = true
```

### B2. No `CMakePresets.json`

**Fix**: Add presets for `debug`, `release`, `sanitize`, `tidy`.

### B3. Missing `LICENSE` file

README says MIT. No license file exists.

**Fix**: Add `LICENSE` file with MIT license text.

### B4. Old-style UHD CMake linking

**Location**: `core/hal/CMakeLists.txt:24-26`

```cmake
target_link_libraries(archerfish_hal PUBLIC ${UHD_LIBRARIES})
target_include_directories(archerfish_hal PUBLIC ${UHD_INCLUDE_DIRS})
```

**Fix**: Use modern imported target `UHD::uhd` if available, or create an imported target wrapper.

### B5. CI: No caching, silent Linux dependency failures

**Location**: `.github/workflows/build.yml`

- No `actions/cache` for Homebrew/build dirs (~2-5 min wasted per run)
- `|| true` on `apt-get install` hides missing dependencies

**Fix**:
- Add `actions/cache` for `~/Library/Caches/Homebrew` and `build/`
- Remove `|| true` from critical deps; let failures be explicit

### B6. Redundant transitive linking in `src/CMakeLists.txt`

**Fix**: Only `archerfish_cli` is needed (brings everything else transitively).

### B7. `schemas/CMakeLists.txt` is a placeholder

**Fix**: Install schema file to `share/archerfish/schemas/`.

### B8. Duplicate `include(CTest)` in test CMakeLists

**Fix**: Remove from `tests/unit/CMakeLists.txt` (already in root).

---

## Documentation Gaps

### D1. Core data model undocumented

`scenario.hpp` (8 structs), `plan.hpp` (3 structs) — the most important types in the codebase — have zero documentation.

**Fix**: Add `///` doc comments to all struct fields.

### D2. SPSC queue threading contract undocumented

The most safety-critical component has no docs on SPSC contract, memory ordering rationale, or usage constraints.

**Fix**: Add detailed doc comments to `spsc_queue.hpp`.

### D3. All 9 impairment classes undocumented

No documentation on what each impairment models, its parameters, or its assumptions.

**Fix**: Add `///` doc comments to each impairment header.

### D4. No file-level comments

No `@file` or brief description of purpose anywhere.

**Fix**: Add 1-2 line file-level comments to all files.

---

## Test Coverage Gaps

### T1. No concurrent SPSC queue tests

Designed for lock-free cross-thread use but all tests are single-threaded.

**Fix**: Add producer-consumer thread test.

### T2. No tests for `cmd_doctor` or `cmd_report`

Two CLI commands have zero test coverage.

### T3. `test_main.cpp` is dead code

Just `REQUIRE(true)`. Remove or repurpose.

### T4. No test fixtures despite repetitive setup

Catch2's `TEST_CASE_METHOD` with fixtures would eliminate repeated configure/prepare patterns.

### T5. No parameterized tests for similar source types

CW/Chirp/Noise/Pulse tests share identical structure. `TEMPLATE_LIST_TEST_CASE` would reduce duplication.

---

## Suggested Implementation Order

### Batch 1 — Quick wins (1-2 hours each)

| Item | Description | LOC Impact |
|------|-------------|------------|
| H2 | Fix `M_PI` → `constants::kPi` | ~20 files touched |
| H3 | Add warning flags | 1 file |
| H4 | Add `.clang-format` | 1 new file |
| M6 | Delete copy on `ISource` | 1 file |
| B1 | Add `.editorconfig` | 1 new file |
| B3 | Add `LICENSE` | 1 new file |
| L10 | Add `[[nodiscard]]` to `render_block` | 1 file |

### Batch 2 — Structural improvements (2-4 hours each)

| Item | Description | LOC Impact |
|------|-------------|------------|
| H1 | Fix `Runtime::active_tx_worker_` dangling pointer | 2-3 files |
| H5 | Deduplicate scenario JSON serialization | ~150 LOC removed |
| M5 | Extract impairment boilerplate to base class | ~54 LOC removed |
| M8 | Add `find_dependency()` to cmake config | 1 file |
| L2 | Extract `add_unit_test()` CMake helper | 360→80 lines |
| B4 | Fix UHD CMake linking | 1 file |

### Batch 3 — Refactoring (4-8 hours each)

| Item | Description | LOC Impact |
|------|-------------|------------|
| H6 | Extract `SourceBase` + `PhaseAccumulator` | ~200 LOC removed |
| M1 | Deduplicate CLI pipeline | 4→1 codepath |
| M2 | Add `configure()` validation | 12 files |
| M3 | Replace sleep with condition variables | 3-4 files |
| M4 | Introduce `enum class WaveformType` | ~10 files |
| M7 | Add sanitizer support + `.clang-tidy` | 2 new files |

### Batch 4 — Polish (defer)

| Item | Description |
|------|-------------|
| L3 | Standardize phase precision to `double` |
| L4 | Standardize sample counter naming |
| L5 | Extract `SourceFactory` |
| L6 | Per-command CLI option structs |
| L7 | Standardize include style |
| L8 | Extract shared CLI format helpers |
| L9 | Decompose `ModulatorSource` |
| B2 | Add `CMakePresets.json` |
| B5-B8 | CI improvements |
| D1-D4 | Documentation |
| T1-T5 | Test coverage |
