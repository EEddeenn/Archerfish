# Archerfish

## Comprehensive Design Specification
### CLI-First Programmable Vector Signal Generator for USRP

Version: Draft 4  
Status: MVP Implemented — Phase 5 Complete

---

## Table of Contents

1. Executive Summary  
2. Product Goals and Non-Goals  
3. Recommended Technical Stack  
4. User Personas and Use Cases  
5. Product Requirements  
6. System Design Principles  
7. Conceptual Model  
8. High-Level Architecture  
9. Runtime Architecture  
10. Concurrency and Threading Model  
11. Hardware Abstraction Layer  
12. Waveform and DSP Architecture  
13. Impairment and Channel Model Architecture  
14. Scenario and Scheduling Architecture  
15. Planning, Compilation, and Execution Model  
16. CLI Product Design  
17. Configuration, Schema, and File Formats  
18. Calibration and Power Model  
19. Reporting, Metrics, and Observability  
20. Safety and Guardrails  
21. Repository and Module Structure  
22. Public Interfaces and Internal Contracts  
23. Error Model  
24. Testing Strategy  
25. Performance Strategy  
26. Packaging and Distribution  
27. Security Considerations  
28. Development Roadmap  
29. MVP Definition  
30. Example Scenario Files  
31. Coding-Agent Prompt  
32. Open Questions  
33. Final Notes
34. Post-MVP Implementation Notes

---

## 1. Executive Summary

**Archerfish** is a **CLI-first programmable vector signal generator** built on top of **USRP hardware** and the **UHD software stack**.

It is designed as a **general-purpose advanced vector signal generation platform** for:

- communications waveform generation,
- receiver and subsystem testing,
- radar / EW / RF sensing prototyping,
- interference generation,
- lab automation,
- hardware-in-the-loop workflows,
- reproducible RF experiments.

Archerfish is intentionally **not** a GNSS-specific simulator in its initial scope. It should instead provide a strong, extensible **general vector signal generator core** that can later support protocol packs and domain-specific waveform modules.

The first and most important product decision is this:

> Archerfish is a **serious CLI instrument backend** before it is anything else.

That means:
- scenario-driven execution,
- deterministic timing where possible,
- clear runtime observability,
- strong validation before transmit,
- machine-friendly automation surfaces,
- architecture designed for later multi-channel, synchronized, and accelerated operation.

---

## 2. Product Goals and Non-Goals

## 2.1 Primary goals

Archerfish should:

1. Provide a clean and modern **CLI application** for RF scene execution.
2. Support **general vector signal generation** rather than narrow protocol specialization.
3. Allow users to define reproducible RF runs using **scenario files**.
4. Support **timed execution**, finite-duration runs, and future-start scheduling.
5. Provide robust waveform generation, replay, and composition.
6. Expose **metrics**, reports, warnings, and errors clearly.
7. Be architected for later:
   - multi-channel TX,
   - coherent transmission,
   - richer impairments,
   - calibration,
   - buffered replay,
   - remote control,
   - optional UI.

## 2.2 Secondary goals

Archerfish should also:
- be pleasant to automate from shell scripts,
- be suitable for CI-style validation and dry runs,
- be suitable for experiment-as-code workflows,
- make common tasks obvious,
- scale from simple tones to advanced mixed scenes.

## 2.3 Non-goals for the initial versions

Archerfish does not initially aim to:
- emulate complete communications protocol stacks end-to-end,
- replace high-end commercial vector signal generators on every RF metric,
- deliver full GPU/FPGA acceleration immediately,
- provide a GUI as the primary operating mode,
- provide GNSS/GPS satellite simulation in the first release.

---

## 3. Recommended Technical Stack

## 3.1 Primary implementation stack

Archerfish should be implemented as:

- **Language:** C++23
- **Build system:** CMake
- **Package/dependency management:** Homebrew + FetchContent
- **CLI framework:** CLI11
- **Logging:** spdlog
- **Formatting:** fmt
- **JSON:** nlohmann/json
- **JSON Schema validation:** pboettch/json-schema-validator (FetchContent)
- **Testing:** Catch2
- **Future RPC:** Protobuf + gRPC
- **Optional scripting:** Python 3 with thin wrapper later

## 3.2 Why this stack

C++ is the correct core language because:
- UHD integration is most natural in C++,
- runtime and DSP paths are performance-sensitive,
- timed execution and streaming are easier to control tightly,
- advanced UHD features remain C++-centric in practice.

Python should be treated as a secondary scripting layer, not the core runtime.

## 3.3 Build and packaging principles

- single top-level CMake project
- dependencies via Homebrew (macOS) / apt (Linux) + FetchContent
- macOS-first + Linux CI
- CI should build release and debug profiles

---

## 4. User Personas and Use Cases

## 4.1 RF / communications engineer
Needs:
- PSK/QAM/FSK/OFDM-style waveforms,
- burst scheduling,
- repeatable test vectors,
- interference generation,
- machine-readable results.

## 4.2 Radar / sensing researcher
Needs:
- CW, chirp, pulse trains, stepped frequency,
- clean timing control,
- finite bursts,
- waveform export and inspection.

## 4.3 SDR / embedded systems developer
Needs:
- easy device discovery,
- deterministic CLI workflows,
- replay of IQ captures,
- integration with Python and shell scripts.

## 4.4 Lab automation / HIL engineer
Needs:
- validated runs,
- future-time execution,
- dry-run planning,
- reports and metrics,
- stable command semantics.

## 4.5 Advanced hobbyist / independent researcher
Needs:
- a modern tool that is easier to use than raw UHD code,
- scenario-driven operation,
- waveform generation commands,
- debuggable behavior.

---

## 5. Product Requirements

## 5.1 Functional requirements

### Device and RF control
Archerfish shall support:
- device enumeration,
- device capability query,
- center frequency control,
- sample rate control,
- analog bandwidth control,
- gain control,
- antenna port control,
- clock source selection,
- time source selection.

### Transmission control
Archerfish shall support:
- continuous transmission,
- finite-duration transmission,
- timed start,
- timed stop,
- future-start execution,
- explicit arm / execute behavior in planned modes.

### Waveform support
Archerfish shall support:
- CW,
- multi-tone,
- pulse and pulse train,
- chirp / LFM / FMCW,
- white noise,
- shaped noise,
- AM / FM / PM,
- ASK / FSK / PSK / QAM / APSK,
- OFDM-like synthesis,
- arbitrary IQ replay.

### Scene support
Archerfish shall support:
- multiple emitters in one scenario,
- per-emitter waveforms,
- per-emitter timing,
- per-emitter impairments,
- gain changes,
- frequency changes,
- timeline-based sequencing.

### Validation
Archerfish shall support:
- schema validation,
- semantic validation,
- capability validation,
- timing validation,
- digital headroom checks,
- missing-file checks.

### Reporting
Archerfish shall support:
- runtime metrics,
- warning summaries,
- execution reports,
- normalized scenario snapshots,
- plan export,
- machine-readable output.

## 5.2 Non-functional requirements

Archerfish should optimize for:
- determinism,
- debuggability,
- observability,
- maintainability,
- extensibility,
- clear failure modes,
- automation ergonomics,
- Linux-first operational stability.

---

## 6. System Design Principles

1. **CLI-first**
   The CLI is the primary interface and should never feel like a second-class wrapper.

2. **Scenario-first**
   The center of the product is the scenario model, not imperative UHD control scripts.

3. **Planning before execution**
   Validation and planning are distinct phases and should remain explicit.

4. **General RF before specialized stacks**
   Keep the signal engine broad and composable.

5. **Deterministic where possible**
   Favor explicit timing semantics, preparation windows, and robust metrics.

6. **Observable by default**
   Every run should explain what happened.

7. **Architecture before optimization**
   Build clean subsystem boundaries before deep acceleration.

8. **No hidden magic**
   Defaults may exist, but they must be visible and inspectable.

9. **Machine-friendly outputs**
   Users should be able to build higher-level systems around Archerfish.

---

## 7. Conceptual Model

Archerfish can be understood through these concepts:

- **Device**: a USRP target
- **Channel**: a logical transmit path bound to a physical TX chain on a device
- **Waveform**: a reusable signal generator definition or sample source
- **Emitter**: a waveform instance attached to a time window and channel
- **Impairment chain**: optional modifications applied to an emitter
- **Scenario**: the top-level description of a run
- **Plan**: the compiled execution result of a scenario
- **Run**: a realized execution with metrics and reports

### Simplified conceptual relationship

```text
Scenario
 ├── Devices
 ├── Channels
 ├── Waveforms
 ├── Emitters
 │    ├── Timing
 │    ├── RF bindings
 │    ├── Signal chain
 │    └── Impairments
 ├── Run configuration
 └── Reporting configuration
```

### Important policy

A waveform may be:
- **inline**, defined directly inside an emitter, or
- **named**, defined in top-level `waveforms` and referenced by ID.

For v1:
- both styles are allowed,
- inline waveforms are self-contained,
- named waveforms are reusable templates,
- emitter-local fields may override only explicitly overridable waveform parameters,
- unresolved conflicts during merge are validation errors.

---

## 8. High-Level Architecture

```text
+------------------------------------------------------------------+
|                            Archerfish                            |
+------------------------------------------------------------------+
|                              CLI Layer                           |
|   Subcommands | Options | Config Loading | Output Formatting     |
+------------------------------------------------------------------+
|                     Scenario / Control Layer                     |
| Parser | Validator | Planner | Resource Allocator | Reporting    |
+------------------------------------------------------------------+
|                         Signal / DSP Layer                       |
| Sources | Modulators | Filters | Impairments | Mixers           |
+------------------------------------------------------------------+
|                      Runtime / Streaming Layer                   |
| TX Sessions | Buffers | Queues | Timing | Metrics | Execution    |
+------------------------------------------------------------------+
|                    Hardware Abstraction Layer                    |
| UHD Wrapper | Device Caps | RF Control | Time/Clock Control      |
+------------------------------------------------------------------+
|                          USRP / UHD Stack                        |
+------------------------------------------------------------------+
```

---

## 9. Runtime Architecture

The runtime is responsible for taking a validated execution plan and turning it into actual transmit activity.

## 9.1 Runtime modes

### Real-time render mode
Waveform samples are rendered during execution.

Use cases:
- dynamic runs,
- moderate complexity scenes,
- interactive development.

Pros:
- flexible,
- lower storage requirements.

Cons:
- more sensitive to host CPU jitter and underruns.

### Pre-rendered host-streaming mode
Waveforms are generated before execution and streamed as prepared buffers.

Use cases:
- stable repeated runs,
- long but predetermined scenes.

Pros:
- more predictable,
- easier runtime behavior.

Cons:
- larger memory/storage pressure,
- less dynamic.

### Replay-buffered mode
Future extension using hardware-assisted replay or deeper buffering.

Use cases:
- more instrument-like behavior,
- longer stable transmissions,
- reduced host sensitivity.

## 9.2 Runtime responsibilities

- create and manage TX sessions,
- reserve runtime resources,
- prime buffers,
- honor timed start/stop,
- process planned timed events,
- surface metrics,
- terminate cleanly,
- write reports.

## 9.3 Runtime states

Suggested state model:

- `Created`
- `Validated`
- `Planned`
- `Prepared`
- `Armed`
- `Running`
- `Completed`
- `Aborted`
- `Failed`

This makes execution status easier to reason about in logs and machine-readable APIs.

---

## 10. Concurrency and Threading Model

This was intentionally absent from the earlier draft and is now specified.

## 10.1 Thread model for MVP

For a single-device, single-channel MVP, use the following threads:

1. **Main/control thread**
   - CLI entry
   - parse / validate / plan
   - prepares runtime
   - owns lifecycle transitions
   - collects final results

2. **Render thread**
   - renders waveform blocks or precomputes segments
   - writes into bounded queue
   - may be bypassed in fully pre-rendered mode

3. **TX worker thread**
   - reads sample blocks from queue
   - feeds UHD TX streamer
   - attaches metadata and burst boundaries
   - records underrun-like runtime conditions

4. **Event/timer dispatcher thread**
   - wakes on planned timed control events
   - submits timed commands or state transitions
   - tracks event dispatch status

## 10.2 Locking policy

- Prefer **single-owner queues** and message passing over shared mutable state.
- Runtime metrics should use:
  - atomics for counters,
  - append-only event logs where practical,
  - explicit lifecycle transitions guarded by one coordinator.
- Avoid coarse global locks in the TX path.

## 10.3 Queue model

Use bounded SPSC or MPSC queues depending on render design.

For MVP:
- one render producer,
- one TX consumer,
- bounded queue with backpressure.

Queue design goals:
- predictable memory use,
- visible queue depth metrics,
- deterministic behavior under pressure.

## 10.4 Timed event dispatch

Timed events should be planned ahead and represented as explicit objects:
- event type,
- target timestamp,
- payload,
- dispatch state,
- completion result.

The event thread must not perform heavy DSP work.

---

## 11. Hardware Abstraction Layer

The HAL isolates UHD-specific details.

## 11.1 Responsibilities

- enumerate devices,
- identify models and channels,
- query capability ranges,
- configure RF parameters,
- configure clock and time sources,
- create and manage TX streamers,
- submit timed device commands,
- expose runtime status.

## 11.2 Required behavior

The rest of Archerfish should never directly depend on UHD-specific classes in its core logic. Instead, it should depend on stable internal interfaces.

## 11.3 Example internal API surface

- `list_devices()`
- `open_device(device_id)`
- `get_capabilities()`
- `set_center_freq(channel, hz)`
- `set_sample_rate(channel, sps)`
- `set_bandwidth(channel, hz)`
- `set_gain(channel, db)`
- `set_antenna(channel, port)`
- `set_clock_source(source)`
- `set_time_source(source)`
- `sync_time_now()`
- `arm_timed_start(time_spec)`
- `start_tx(channel)`
- `stop_tx(channel)`
- `query_runtime_status()`

## 11.4 Capability model

Each device should expose:
- supported channels,
- frequency range,
- rate range,
- gain range,
- bandwidth options or range,
- time/clock source support,
- streamer limitations,
- optional replay support in future.

---

## 12. Waveform and DSP Architecture

The DSP layer should be modular and graph-oriented.

## 12.1 Signal source types

### Primitive generators
- CW
- constant complex
- multi-tone
- chirp
- pulse
- pulse train
- noise

### Digital modulation generators
- ASK
- FSK
- BPSK / QPSK / M-PSK
- QAM
- APSK
- OFDM-like synthesis

### File-backed generators
- IQ replay from supported sample files
- looping replay
- finite replay
- sample rate and metadata awareness

## 12.2 Processing blocks

- resampler
- pulse shaper
- scaler
- envelope block
- gating block
- summer / mixer
- format conversion block

## 12.3 Block model

All waveforms should conform to a common conceptual rendering interface:
- configure
- prepare
- render block
- report metadata
- reset

### 12.3.1 Implemented source interface (`ISource`)

The implemented `ISource` abstract base class provides the following methods:
- `configure(params)` — set waveform parameters from parsed scenario data
- `prepare()` — precompute constants, build filters, run measurement pass
- `render_block(buf, n)` — generate `n` complex samples into `buf`
- `report_metadata()` — return waveform metadata (peak, RMS, crest factor, sample rate)
- `reset()` — restore source to initial state, clearing filter tail and sample counters

`ModulatorSource` (PSK/QAM) additionally exposes a `constellation()` const accessor for unit test verification of symbol-to-constellation mapping.

### 12.3.2 Source base class (`SourceBase`)

A shared base class `SourceBase` was extracted from the 12 concrete source implementations, eliminating ~200 lines of duplicated boilerplate. All concrete sources now inherit from `SourceBase` instead of directly from `ISource`.

`SourceBase` provides:
- `configure_common(params)` — extract amplitude, sample_rate, duration_sec, seed from JSON
- `compute_block_size(max_samples)` — duration-aware block sizing (replaces the 8-line preamble in every source)
- `fill_common_metadata(meta)` — populate common WaveformMetadata fields
- `reset_common()` — reset shared counters
- `validate_positive()` / `validate_non_negative()` — input validation helpers
- Accessors: `amplitude()`, `sample_rate()`, `samples_produced()`, `seed()`

## 12.4 Sample format support

Suggested internal canonical format:
- `complex<float>`

Possible external / file formats later:
- `complex<int16>`
- `complex<float>`
- `complex<double>`

## 12.5 Waveform metadata

Each waveform should expose:
- nominal bandwidth,
- sample rate,
- amplitude limits,
- crest factor estimate where meaningful,
- duration semantics,
- repeat semantics.

### 12.5.1 Calibrated peak/RMS metadata (implemented)

Rather than using a hardcoded crest factor estimate, the implementation performs a 256-symbol measurement pass during `prepare()`. This renders 256 symbols of the modulated signal (without amplitude scaling) and measures the actual peak and RMS values. The measured peak/RMS are then stored and reported via `report_metadata()`, giving accurate crest factor values for each modulation order. CW, chirp, noise, and file sources use analytically known values where applicable.

## 12.6 Sample rate negotiation and resampling

This is now explicit.

There are three sample-rate domains:
1. waveform-native rate,
2. emitter/requested rate,
3. device/channel transmit rate.

Policy for v1:
- one device TX rate per bound channel,
- if waveform-native rate equals device rate, no resampling needed,
- if waveform-native rate differs and a supported resampling path exists, the DSP layer performs explicit resampling,
- if no valid resampling path exists, validation fails.

The planner must record:
- source rate,
- target rate,
- resampling ratio,
- expected cost.

Resampling is never implicit in a hidden way.

## 12.7 Constellation mapping (implemented)

### 12.7.1 Gray-coded constellations

Digital modulation constellations (QPSK, 8-PSK, 16-QAM, 64-QAM) use Gray coding, following the conventions of liquid-dsp and GNU Radio. Adjacent constellation points differ by exactly one bit, minimizing bit error rate in additive noise.

For QAM orders, the constellation is built on independent I and Q axes:
- Each axis uses a Gray-coded PAM sub-constellation
- A `gray_decode()` function converts Gray-coded indices to natural binary indices independently per axis
- The two decoded axes are combined to form the complex symbol

Normalization factors ensure unit average symbol energy:
- 16-QAM: normalized with alpha = 1/sqrt(10)
- 64-QAM: normalized with alpha = 1/sqrt(42)

These factors are derived from the average power of the rectangular QAM grid before normalization.

### 12.7.2 APSK constellation mapping (DVB-S2)

APSK (Amplitude Phase Shift Keying) constellations follow DVB-S2 standard geometry:
- **16-APSK**: 2 rings — 4 inner points (r₁) + 12 outer points (r₂), with γ = r₂/r₁ = 2.85
- **32-APSK**: 3 rings — 4 inner (r₁) + 12 middle (r₂) + 16 outer (r₃), normalized to unit average energy

Ring radii are derived from the DVB-S2 specification. Bit mapping follows the DVB-S2 standard. Normalization ensures unit average symbol energy across all constellation points.

## 12.8 WaveformType enum

All waveform type dispatch uses a typed `WaveformType` enum class rather than string comparisons. The enum covers 19 waveform types (CW, Chirp, Noise, BPSK through QAM64, APSK16, APSK32, MultiTone, File, Pulse, ASK, FSK, AM, FM, PM, OFDM) plus an `Unknown` sentinel.

Conversion functions:
- `to_string(WaveformType)` — canonical lowercase name
- `waveform_type_from_string(string_view)` — `std::expected<WaveformType, string>` with alias support (e.g. "16qam" → QAM16)
- `waveform_type_cli_name(WaveformType)` — uppercase CLI display name

The `SourceFactory` uses switch-based dispatch on `WaveformType` instead of string if-chains.

## 12.9 OFDM synthesis source

`OfdmSource` generates multi-subcarrier OFDM waveforms using an in-house Cooley-Tukey radix-2 IFFT. Configuration parameters:
- `fft_size` — IFFT size (must be power of 2)
- `cyclic_prefix_size` — samples copied from symbol tail as prefix
- `active_subcarriers` — number of subcarriers carrying data
- `amplitude` — output scaling

Each OFDM symbol:
1. Generate random QPSK symbols for active subcarriers
2. Map to IFFT bins (center subcarrier and edges are zero-padded)
3. IFFT to time domain (in-place, radix-2)
4. Append cyclic prefix
5. Concatenate symbols for the configured duration

No external FFT library dependency. Normalized to unit average symbol energy.

## 12.10 Pulse shaping and filter continuity (implemented)

### 12.10.1 Root raised cosine (RRC) pulse shaper

The RRC filter uses `constexpr kPi` instead of `M_PI` for Windows portability, ensuring the code compiles without relying on POSIX-defined math constants.

### 12.10.2 Inter-batch filter continuity

The modulator source uses an overlap-save scheme to maintain filter state across `render_block()` calls. A `filter_tail_` buffer preserves the trailing samples from the FIR impulse response between consecutive block renders. This prevents discontinuities at batch boundaries that would otherwise appear as spectral splatter or transient artifacts in the output signal.

---

## 13. Impairment and Channel Model Architecture

Archerfish is not a full channel emulator in v1, but it should have a clean impairment architecture.

## 13.1 Initial impairment set

- AWGN
- CFO
- phase offset
- IQ gain imbalance
- IQ phase imbalance
- DC offset
- scalar amplitude ripple
- delay

## 13.2 Later impairments

- phase noise approximation
- multipath tap model
- fading approximations
- PA nonlinearity models
- burst dropouts / glitches
- scheduled impairment state changes

## 13.3 Impairment model design

Impairments should be:
- attachable per emitter,
- serializable in scenarios,
- composable in a stable order,
- individually enable/disable-able,
- inspectable in planning output.

---

## 14. Scenario and Scheduling Architecture

This is the center of the product.

## 14.1 Scenario responsibilities

A scenario should define:
- what to transmit,
- where to transmit,
- when to transmit,
- how to impair it,
- how to report results.

## 14.2 Channels model

The earlier draft listed a `channels` section without defining it. This is now clarified.

A scenario may define channels in two ways:

### Simple mode
A device entry may directly include one `channel` field for MVP convenience.

### Explicit mode
A top-level `channels` list may define logical channel objects with:
- `id`
- `device`
- `index`
- RF defaults
- calibration binding
- optional future sync group

For MVP:
- simple mode is supported and preferred in examples,
- explicit mode is reserved but documented,
- internal normalization converts both into the same channel model.

## 14.3 Timing model

Support:
- immediate runs,
- delayed runs,
- timed runs,
- finite-duration runs,
- repeated burst schedules,
- ordered event sequences.

## 14.4 Anchor for `start_after_sec`

This is now defined.

For CLI execution:
- `start_after_sec` is anchored to the **scenario execution epoch**,
- the scenario execution epoch is defined as the instant the runtime enters `Prepared` and commits to launch timing,
- this epoch is global within the run, not per-device.

Therefore:
- all `start_after_sec` values in one scenario are relative to one shared run epoch,
- the planner converts them into device-relative target times during preparation.

## 14.5 Event types

Implemented event classes:
- retune (frequency change at scheduled time)
- gain_change (gain adjustment at scheduled time)
- marker (named timestamp for synchronization/logging)
- burst (burst trigger)

Future event types:
- waveform switch
- impairment enable/disable

## 14.6 Multi-emitter policy on a single channel

This was ambiguous before. It is now explicit.

For Archerfish:
- multiple emitters **may** target the same channel,
- if their active windows do not overlap, execution is sequential and trivial,
- if their active windows overlap, the default composition policy is **additive mixing**,
- additive mixing is supported only when the planner can prove the composition is valid for that runtime mode.

For MVP:
- overlapping emitters on one channel are **allowed only in pre-rendered or explicitly mixer-capable paths**,
- otherwise the planner rejects the scenario as unsupported.

The planner must compute:
- peak sum bounds,
- expected digital headroom,
- required mixing path,
- whether the overlap is supported in the selected runtime mode.

If sum amplitude risks clipping:
- planner emits warning or error depending on policy.

### Additive mixing (implemented)

When emitters on the same channel have overlapping time windows and `mixing` is set to `"additive"`:
- The planner groups overlapping emitters into `MixGroup` structures
- Each MixGroup records: device_id, channel, time window, emitter_ids, estimated peak sum
- The render worker renders each emitter independently, then sums element-wise
- If the estimated peak sum exceeds 1.0, a headroom warning is emitted

Emitters without explicit `mixing` mode are handled sequentially (non-overlapping windows enforced by validator, as before).

---

## 15. Planning, Compilation, and Execution Model

Archerfish should separate these phases:

### Parse
Read and normalize user input.

### Validate
Check schema, semantics, resources, and capabilities.

### Plan
Resolve timing, channel allocation, execution windows, resource use, and required DSP operations.

### Prepare
Open devices, preload buffers, pre-render if needed, reserve runtime state.

### Execute
Run according to plan and collect runtime metrics.

### Report
Persist metrics, warnings, normalized scenario, and plan outputs.

## 15.1 Why explicit planning matters

It improves:
- debuggability,
- reproducibility,
- machine integration,
- future buffered and distributed execution,
- user trust.

## 15.2 Plan object contents

A resolved plan should contain:
- normalized RF settings,
- resolved timing windows,
- waveform render instructions,
- emitter-channel bindings,
- preparation requirements,
- expected resource usage,
- warning list.

## 15.3 Planning output format

`archerfish scenario plan` shall emit:
- human-readable summary by default,
- JSON when `--json` is passed.

The JSON plan should include:
- normalized scenario,
- channel bindings,
- timeline events,
- resampling decisions,
- overlap decisions,
- warnings,
- estimated runtime mode requirements.

---

## 16. CLI Product Design

## 16.1 Command layout

```bash
archerfish devices list
archerfish devices info --device usrp0

archerfish scenario validate scenario.json
archerfish scenario plan scenario.json
archerfish scenario dry-run scenario.json
archerfish scenario run scenario.json

archerfish wave gen cw ...
archerfish wave gen chirp ...
archerfish wave gen qpsk ...
archerfish wave gen apsk16 --symbol-rate ... --sps ... --rrc ... --duration ... --amplitude ... --output ...
archerfish wave gen apsk32 --symbol-rate ... --sps ... --rrc ... --duration ... --amplitude ... --output ...
archerfish wave gen ofdm --rate ... --duration ... --amplitude ... --output ...
archerfish wave inspect file.cf32

archerfish calib init --device usrp0 [--channel <n>]
archerfish calib show --device usrp0 [--channel <n>] [--json]
archerfish calib import calib.json --device <id> [--channel <n>]

archerfish report show run_001.json
archerfish metrics export --latest

archerfish doctor
archerfish version
archerfish schema print [--json] [--markdown]
```

## 16.2 CLI UX requirements

The CLI should support:
- `--json` output where practical,
- `--verbose`,
- `--quiet`,
- explicit exit codes,
- explicit device selection,
- dry-run planning,
- human and machine readable output modes.

## 16.3 Exit code philosophy

Suggested categories:
- `0` success
- `1` generic failure
- `2` input / validation failure
- `3` planning failure
- `4` device / runtime preparation failure
- `5` execution failure

Optional future refinements:
- `6` underrun-detected completion
- `7` device disconnect
- `8` timeout/cancellation

## 16.4 `wave gen` specification

For MVP, the following subcommands should be supported:

### CW
```bash
archerfish wave gen cw --rate 10e6 --duration 0.1 --amplitude 0.2 -o cw.cf32
```

### Chirp
```bash
archerfish wave gen chirp --rate 20e6 --duration 0.01 --f0 -1e6 --f1 1e6 --amplitude 0.3 -o chirp.cf32
```

### QPSK
```bash
archerfish wave gen qpsk --symbol-rate 1e6 --sps 8 --rrc 0.35 --duration 0.05 --amplitude 0.2 -o qpsk.cf32
```

Expected output:
- waveform file,
- optional sidecar metadata JSON,
- console summary or JSON summary.

## 16.5 `wave inspect` specification

`wave inspect` should report:
- file format,
- sample count,
- duration,
- inferred or sidecar sample rate,
- peak amplitude,
- RMS level,
- crest factor estimate.

When `--json` is passed, it should emit structured metadata.

---

## 17. Configuration, Schema, and File Formats

## 17.1 Scenario files

Use JSON first. YAML may be supported later.

### Top-level sections
- `metadata`
- `devices`
- `channels`
- `waveforms`
- `emitters`
- `run`
- `reporting`

## 17.2 `waveforms` section semantics

A top-level `waveforms` section contains reusable waveform definitions.

Example:
```json
{
  "waveforms": [
    {
      "id": "qpsk_base",
      "type": "qpsk",
      "symbol_rate": 1000000.0,
      "samples_per_symbol": 8,
      "rrc_alpha": 0.35,
      "amplitude": 0.2
    }
  ]
}
```

Emitters may reference them:
```json
{
  "waveform_ref": "qpsk_base"
}
```

Emitter-local override policy:
- safe scalar fields like amplitude and duration-related shaping options may override if allowed,
- type changes are forbidden,
- invalid override combinations fail validation.

## 17.3 Example minimal scenario

```json
{
  "metadata": {
    "name": "future_start_cw"
  },
  "devices": [
    {
      "id": "usrp0",
      "channel": 0,
      "rf": {
        "freq_hz": 2450000000.0,
        "rate_sps": 10000000.0,
        "gain_db": 20.0
      }
    }
  ],
  "emitters": [
    {
      "id": "cw1",
      "device": "usrp0",
      "channel": 0,
      "start_after_sec": 3.0,
      "duration_sec": 5.0,
      "waveform": {
        "type": "cw",
        "amplitude": 0.2
      }
    }
  ]
}
```

## 17.4 Example advanced mixed scenario

```json
{
  "metadata": {
    "name": "mixed_scene_demo"
  },
  "devices": [
    {
      "id": "usrp0",
      "channel": 0,
      "rf": {
        "freq_hz": 915000000.0,
        "rate_sps": 20000000.0,
        "gain_db": 18.0
      }
    }
  ],
  "waveforms": [
    {
      "id": "qpsk_base",
      "type": "qpsk",
      "symbol_rate": 1000000.0,
      "samples_per_symbol": 8,
      "rrc_alpha": 0.35,
      "amplitude": 0.2
    }
  ],
  "emitters": [
    {
      "id": "chirp_a",
      "device": "usrp0",
      "channel": 0,
      "start_after_sec": 1.0,
      "duration_sec": 0.02,
      "waveform": {
        "type": "chirp",
        "f0_hz": -2000000.0,
        "f1_hz": 2000000.0,
        "amplitude": 0.3
      }
    },
    {
      "id": "qam_burst",
      "device": "usrp0",
      "channel": 0,
      "start_after_sec": 2.0,
      "duration_sec": 0.10,
      "waveform_ref": "qpsk_base",
      "impairments": {
        "cfo_hz": 200.0,
        "iq_gain_imbalance_db": 0.2
      }
    }
  ],
  "reporting": {
    "save_plan": true,
    "save_metrics": true
  }
}
```

### 17.4.1 Scenario with events and mixing

```json
{
  "metadata": { "name": "advanced_scene" },
  "devices": [
    { "id": "usrp0", "channel": 0, "rf": { "freq_hz": 2450000000.0, "rate_sps": 20000000.0, "gain_db": 20.0 } }
  ],
  "emitters": [
    {
      "id": "cw1", "device": "usrp0", "channel": 0,
      "start_after_sec": 0.0, "duration_sec": 1.0,
      "waveform": { "type": "cw", "amplitude": 0.2 },
      "mixing": "additive"
    },
    {
      "id": "cw2", "device": "usrp0", "channel": 0,
      "start_after_sec": 0.5, "duration_sec": 1.0,
      "waveform": { "type": "cw", "amplitude": 0.15 },
      "mixing": "additive"
    },
    {
      "id": "pulse_train", "device": "usrp0", "channel": 0,
      "start_after_sec": 2.0, "duration_sec": 0.001,
      "waveform": { "type": "pulse", "amplitude": 0.5 },
      "repeat": { "count": 5, "interval_sec": 0.01 }
    }
  ],
  "events": [
    { "target_device": "usrp0", "time_sec": 1.5, "type": "marker", "payload": { "label": "switch_point" } },
    { "target_device": "usrp0", "time_sec": 2.0, "type": "gain_change", "payload": { "gain_db": 25.0 } }
  ]
}
```

## 17.5 Waveform files

Possible future file types:
- raw CF32
- raw CI16
- sidecar metadata JSON

---

## 18. Calibration and Power Model

Archerfish should have a calibration path even if the first release only provides basic estimates.

## 18.1 Goals

- estimate approximate RF output power,
- track calibration by device and channel,
- support gain/frequency dependent calibration tables,
- warn about clipping and crest factor,
- provide a future path toward target-power workflows.

## 18.2 Calibration data model

Calibration should be stored in JSON for v1.

Suggested shape:
```json
{
  "device_serial": "XYZ123",
  "channel": 0,
  "entries": [
    {
      "freq_hz": 915000000.0,
      "gain_db": 20.0,
      "rate_sps": 10000000.0,
      "digital_amplitude": 0.2,
      "measured_power_dbm": -12.5
    }
  ]
}
```

## 18.3 Calibration file location

Suggested defaults:
- user-local: `~/.config/archerfish/calibration/`
- project-local override via CLI flag
- explicit import/export commands

## 18.4 User-facing behavior

Archerfish should:
- show estimated dBm where possible,
- clearly mark estimated vs calibrated values,
- expose waveform peak and average levels where useful,
- warn on digital overdrive risks.

Planner behavior:
- calibration may influence report quality and target-power planning,
- calibration is not mandatory for MVP execution unless explicitly requested by mode.

---

## 19. Reporting, Metrics, and Observability

## 19.1 Why this matters

Archerfish is an engineering instrument backend. A run without inspectable metrics is incomplete.

## 19.2 Metrics to collect

- start time requested
- start time actual
- stop time actual
- TX active duration
- underrun count
- late command count
- queue depth statistics
- warning count
- error count
- plan hash / scenario hash

## 19.3 Report format

For v1, reports are JSON files.

Suggested output layout:
```text
runs/
  2026-04-03T120000Z_future_start_cw/
    scenario.normalized.json
    plan.json
    report.json
    metrics.json
    logs.txt
```

`report show` should read `report.json` and print:
- summary status,
- timing summary,
- key warnings,
- device info,
- artifact paths.

## 19.4 Logging

Provide:
- human logs,
- debug logs,
- optional structured logs later.

---

## 20. Safety and Guardrails

This is a transmit-capable RF tool. The software should reduce accidental misuse.

Recommended guardrails:
- explicit transmit arming semantics,
- warnings on likely regulated bands,
- high-power warning thresholds,
- dry-run mode by default in some workflows,
- clear reporting of actual RF settings,
- optional lab-safe configuration profiles later.

---

## 21. Repository and Module Structure

Repository root is `archerfish/`. The earlier ambiguity is removed.

```text
archerfish/
├── CMakeLists.txt
├── docs/
│   ├── Design.md
│   └── plans/
├── schemas/
│   └── scenario.schema.json
├── core/
│   ├── hal/
│   ├── runtime/
│   ├── dsp/
│   ├── impairments/
│   ├── scheduler/
│   ├── reporting/
│   └── common/
├── cli/
├── examples/
├── tests/
│   ├── unit/
│   └── integration/
```

**Notes on current state:**
- `schemas/scenario.schema.json` exists and covers all Phase 1 and Phase 2 waveform types and impairments.
- `presets/` directory has been removed; future preset templates will be added as needed.
- `tools/` and `packaging/` directories are not present in the current codebase and are deferred to a later phase.

---

## 22. Public Interfaces and Internal Contracts

## 22.1 Internal boundaries

Keep these subsystems distinct:
- CLI parsing
- scenario parse/validate/plan
- runtime execution
- waveform rendering
- HAL / UHD integration
- reporting

## 22.2 Example internal contracts

### Scenario parser output
A normalized in-memory scenario model.

### Validator output
A set of errors/warnings plus normalized scenario.

### Planner output
A plan object with resolved timing, resources, overlap policy decisions, and execution instructions.

### Runtime output
Metrics, warnings, error state, and artifact paths.

---

## 23. Error Model

Errors should be explicit and categorized.

## 23.1 Categories

### Configuration error
- malformed scenario
- missing required field
- invalid CLI arguments
- unsupported waveform parameters

### Validation error
- impossible duration
- invalid amplitude
- missing file
- unsupported field combinations

### Planning error
- resource collision
- timing conflict
- unsupported rate/bandwidth combination
- digital headroom violation
- unsupported overlap composition mode

### Runtime preparation error
- USRP unavailable
- stream creation failed
- preparation time too short
- missing calibration data if required by mode

### Runtime execution error
- underrun
- late command
- stream failure
- device disconnect

### Quality warning
- clipping risk
- uncalibrated estimate
- degraded timing certainty
- unsupported idealized assumption

---

## 24. Testing Strategy

## 24.1 Unit tests
- waveform math
- modulation blocks
- parser behavior
- validator rules
- planner logic
- calibration interpolation

## 24.2 Integration tests
- end-to-end scenario parse/validate/plan
- report generation
- replay preparation
- dry-run behavior

## 24.3 Hardware tests
- basic TX smoke test
- future-start timing test
- burst execution test
- chirp transmission test
- underrun handling behavior

## 24.4 Golden artifact tests

Use:
- metadata snapshots,
- checksums,
- compact generated references,
- not large binary archives in-repo unless justified.

---

## 25. Performance Strategy

Archerfish should plan for performance without compromising architecture.

## 25.1 First-stage performance priorities
- efficient block rendering,
- low-copy buffer flow,
- stable TX worker architecture,
- measurable queue health,
- sensible pre-render options.

## 25.2 Later optimization areas
- precompiled waveform segments,
- replay buffering,
- hardware-assisted paths,
- vectorized DSP,
- selective acceleration of bottleneck blocks.

---

## 26. Packaging and Distribution

## 26.1 Packaging goals
- Linux-first support,
- reproducible builds,
- easy dependency installation,
- clear environment diagnostics.

## 26.2 Developer ergonomics
Provide:
- one-step configure/build docs,
- `archerfish doctor`,
- example scenarios,
- sample waveform files,
- CI-based binary artifacts later.

---

## 27. Security Considerations

While Archerfish is a local engineering tool, it still benefits from:
- careful file handling,
- explicit path resolution,
- structured logging without accidental sensitive leakage,
- no arbitrary code execution in scenario files,
- safe defaults in report writing and file output,
- scenario size limits,
- recursion/depth limits in parser structures,
- resource budgets to prevent denial-of-service from pathological input.

---

## 28. Development Roadmap

## Phase 1: CLI MVP — DONE
- [x] CLI skeleton
- [x] device enumeration
- [x] one-channel TX
- [x] CW, chirp, noise, QPSK/QAM, replay
- [x] parser, validator, planner
- [x] scenario run
- [x] reports and metrics

## Phase 2: Waveform expansion and schema validation — DONE
- [x] JSON Schema file (`scenario.schema.json`) for formal scenario validation
- [x] AM / FM / PM analog modulation sources
- [x] ASK / FSK digital modulation sources
- [x] Pulse and pulse train generators
- [x] Resampler block for non-integer sample rate conversion (libsamplerate)
- [x] Sidecar metadata JSON output from `wave gen`
- [x] Enhanced impairments (amplitude ripple, delay, burst dropout)
- [x] Reporting improvements (emitter metrics, `report show`)
- [x] Scenario dry-run with ASCII timeline
- [x] APSK constellation support (16-APSK, 32-APSK for DVB-S2)
- [x] OFDM-like synthesis (in-house Cooley-Tukey FFT, cyclic prefix)
- [x] `calib init` / `calib show` / `calib import` commands
- [x] `schema print` command for dumping the scenario JSON Schema
- [x] Multi-emitter composition with additive mixing

## Phase 3: Codebase Hardening — DONE
- [x] Warning enforcement (`-Wall -Wextra -Wpedantic -Werror`)
- [x] ISource copy protection and `[[nodiscard]]` on render_block()
- [x] SourceBase extraction (~200 LOC removed across 12 sources)
- [x] WaveformType enum replacing string-based dispatch
- [x] Condition variable thread coordination (replaces sleep-based polling)
- [x] MIT LICENSE file
- [x] CMake sanitizer options (ASAN, UBSAN)
- [x] CMake package config with find_dependency calls

## Phase 4: Phase 2 Completion — DONE
- [x] `schema print` command (human-readable, --json, --markdown)
- [x] APSK constellations (16-APSK, 32-APSK per DVB-S2)
- [x] Multi-emitter additive mixing with headroom diagnostics
- [x] Calibration commands (init/show/import)
- [x] OFDM synthesis (radix-2 FFT, cyclic prefix, configurable subcarriers)

## Phase 5: Scheduling Sophistication — DONE
- [x] Timed retune and gain change events
- [x] Repeated burst scheduling (count + interval)
- [x] Marker events for synchronization/logging
- [x] Planning diagnostics (CPU load, memory, timing feasibility)

## Phase 6: Multi-channel
- multi-channel TX
- same-device alignment
- sync metadata
- channel-level metrics

## Phase 7: Performance and advanced features
- buffered replay
- stronger calibration
- richer impairments
- remote API
- possible UI layer

---

## 29. MVP Definition

A valid MVP must support all of the following:

1. [x] `archerfish devices list`
2. [x] `archerfish devices info`
3. [x] `archerfish scenario validate`
4. [x] `archerfish scenario plan`
5. [x] `archerfish scenario run`
6. [x] CW future-start run
7. [x] Chirp burst run
8. [x] QPSK/QAM run
9. [x] IQ replay run
10. [x] Runtime metrics and saved report

All 10 items are implemented.

### MVP limits made explicit

For MVP:
- one physical device
- one TX channel
- overlapping emitters only if using supported pre-rendered mixing path
- no mandatory calibration
- JSON only
- Linux-first

---

## 30. Example Scenario Files

## 30.1 Future-start CW

```json
{
  "metadata": { "name": "future_start_cw" },
  "devices": [
    {
      "id": "usrp0",
      "channel": 0,
      "rf": {
        "freq_hz": 2450000000.0,
        "rate_sps": 10000000.0,
        "gain_db": 20.0
      }
    }
  ],
  "emitters": [
    {
      "id": "cw1",
      "device": "usrp0",
      "channel": 0,
      "start_after_sec": 2.0,
      "duration_sec": 4.0,
      "waveform": {
        "type": "cw",
        "amplitude": 0.2
      }
    }
  ]
}
```

## 30.2 Chirp burst

```json
{
  "metadata": { "name": "chirp_burst" },
  "devices": [
    {
      "id": "usrp0",
      "channel": 0,
      "rf": {
        "freq_hz": 915000000.0,
        "rate_sps": 20000000.0,
        "gain_db": 18.0
      }
    }
  ],
  "emitters": [
    {
      "id": "chirp1",
      "device": "usrp0",
      "channel": 0,
      "start_after_sec": 1.0,
      "duration_sec": 0.02,
      "waveform": {
        "type": "chirp",
        "f0_hz": -2000000.0,
        "f1_hz": 2000000.0,
        "amplitude": 0.3
      }
    }
  ]
}
```

## 30.3 QPSK burst

```json
{
  "metadata": { "name": "qpsk_burst" },
  "devices": [
    {
      "id": "usrp0",
      "channel": 0,
      "rf": {
        "freq_hz": 2400000000.0,
        "rate_sps": 8000000.0,
        "gain_db": 15.0
      }
    }
  ],
  "emitters": [
    {
      "id": "qpsk1",
      "device": "usrp0",
      "channel": 0,
      "start_after_sec": 1.5,
      "duration_sec": 0.10,
      "waveform": {
        "type": "qpsk",
        "symbol_rate": 1000000.0,
        "samples_per_symbol": 8,
        "rrc_alpha": 0.35,
        "amplitude": 0.2
      }
    }
  ]
}
```

---

## 31. Coding-Agent Prompt

Copy the following prompt into your coding agent.

---

# Prompt: Build Archerfish as a CLI-First Vector Signal Generator for USRP

You are building **Archerfish**, a **CLI-first programmable vector signal generator for USRP**.

This is a general-purpose advanced vector signal generator for:
- communications waveform generation,
- radar / sensing prototyping,
- receiver testing,
- interference generation,
- hardware-in-the-loop workflows,
- lab automation.

It is **not** a GNSS/GPS-specialized simulator at this stage.

## Product goals

Build a serious RF engineering tool that is:
- scenario-driven,
- deterministic,
- observable,
- extensible,
- machine-friendly,
- architecturally clean.

## Immediate MVP requirements

Implement:
- CLI app named `archerfish`
- C++23 + CMake + Homebrew stack
- USRP abstraction over UHD
- one-device, one-channel TX
- support for CW, multi-tone, chirp, noise, QPSK/QAM, and IQ replay
- scenario JSON parser
- schema and semantic validation
- scenario planning
- scenario execution
- runtime metrics and reports
- example scenarios and tests

## Required module boundaries

- `core/hal/`
- `core/runtime/`
- `core/dsp/`
- `core/impairments/`
- `core/scheduler/`
- `core/reporting/`
- `cli/`
- `schemas/`
- `examples/`
- `tests/`

## Non-negotiable rules

- CLI is the primary interface.
- Keep UHD-specific logic inside HAL.
- Keep planning separate from execution.
- Keep waveform rendering separate from TX streaming.
- All runs should be reproducible from scenario files.
- Use explicit typed structures.
- Build strong error categories.
- Produce machine-readable reports.
- Leave clean extension points for multi-channel sync, calibration, replay buffering, and richer impairments.

## Priority commands

- `archerfish devices list`
- `archerfish devices info --device <id>`
- `archerfish scenario validate <file>`
- `archerfish scenario plan <file>`
- `archerfish scenario run <file>`
- `archerfish wave gen ...`
- `archerfish wave inspect <file>`
- `archerfish doctor`

## Implementation order

1. CLI skeleton
2. HAL
3. runtime and thread model
4. waveform generators
5. parser
6. validator
7. planner
8. runner
9. reporting
10. tests and docs

## Output expectations

Produce:
- a clean repository layout,
- buildable code,
- example scenarios,
- unit and integration tests,
- architecture notes,
- schema docs,
- CLI usage docs.

## Quality expectations

- production-style structure
- clear naming
- robust errors
- no giant god-object
- minimal hidden behavior
- future-proof interfaces
- comments where architectural intent matters

## Final instruction

Think like you are building the backend of a real RF instrument, not a demo. Optimize for architecture, CLI ergonomics, determinism, inspectability, and extension into a larger platform.

---

## 32. Open Questions

These are intentionally left as explicit future decisions rather than hidden ambiguity:

1. Which resampler implementation should back the DSP layer in v1?
2. ~~Should v1 support one mixed overlapping-emitter path, or reject all overlap until Phase 2?~~ **Resolved:** Overlap is allowed when emitters specify `"mixing": "additive"`. The planner groups overlapping emitters into `MixGroup` structures and renders them additively. Non-additive overlap is still rejected by the validator.
3. What exact waveform sidecar metadata format should `wave gen` emit?
4. Which JSON schema validator library best balances strictness and maintenance burden?
5. When remote API work begins, should gRPC be embedded in the same process or in a service wrapper?
6. Should Phase 2 implement AM/FM/PM as source types (like ModulatorSource) or as impairment-like wrappers that modulate an existing source?
7. What resampler architecture should be used for non-integer sample rate ratios? Options include polyphase FIR, rational resampler (interpolate-then-decimate), or an arbitrary resampler (e.g., polyphase with fractional phase accumulator).

---

## 33. Final Notes

Archerfish should begin as a **strong CLI RF engine** with a disciplined architecture.

If built well, it can later grow into:
- a richer waveform compiler,
- a more advanced scene scheduler,
- a calibrated lab source,
- a multi-channel coherent transmitter,
- a remotely controlled RF instrument platform.

---

## 34. Post-MVP Implementation Notes

This section documents implementation decisions made during Phase 1 that go beyond what was specified in the original design. These are now part of the implemented baseline.

### 34.1 Gray-coded constellations

The digital modulation constellations were implemented with Gray coding from the start, following liquid-dsp and GNU Radio conventions. This was not explicitly required in Draft 2 but was included as a correctness measure. Adjacent constellation points differ by exactly one bit, and QAM orders use independent per-axis Gray decoding.

### 34.2 Overlap-save inter-batch filter continuity

The ModulatorSource uses an overlap-save scheme to maintain FIR filter state across `render_block()` calls. A `filter_tail_` buffer preserves trailing filter samples between blocks, eliminating boundary artifacts. This is a signal integrity measure that prevents spectral splatter at block edges during long renders.

### 34.3 Calibrated crest factor and RMS via measurement pass

Rather than using a hardcoded peak-to-RMS ratio (such as sqrt(2)), the implementation runs a 256-symbol measurement pass during `prepare()` for modulated sources. This renders a representative segment of the modulated signal, measures actual peak and RMS values, and stores them for metadata reporting. The result is accurate crest factor values per modulation order rather than rough estimates.

### 34.4 Windows portability (constexpr kPi)

The RRC pulse shaper and other DSP blocks use `constexpr kPi` instead of the POSIX `M_PI` macro. This ensures the code compiles cleanly on MSVC and other Windows toolchains where `M_PI` is not defined by default. The constant is defined once and reused across the DSP layer.

### 34.5 SourceBase extraction (Phase 3)

All 12 concrete ISource implementations were refactored to inherit from a shared `SourceBase` base class. This eliminated ~200 lines of duplicated code across the DSP layer. Common fields (amplitude, sample_rate, duration_sec, seed, samples_produced) and common behaviors (duration-aware block sizing, metadata filling, reset) are now in one place. Each concrete source overrides only type-specific configure/render logic.

### 34.6 WaveformType enum (Phase 3)

String-based waveform dispatch was replaced with a typed `WaveformType` enum class covering 19 waveform types plus an `Unknown` sentinel. The `SourceFactory` uses switch-based dispatch instead of string if-chains. The `WaveformDef::type` field changed from `std::string` to `dsp::WaveformType`, propagating type safety through the scheduler, runtime, and CLI layers.

### 34.7 Condition variable queue coordination (Phase 3)

The SPSC queue was augmented with condition variable methods (`push_notify()`, `pop_wait(stop_token)`, `push_wait(stop_token, item)`) replacing sleep-based polling in the TX worker and render worker. A `std::stop_source` enables cooperative cancellation of blocking waits, and lost-wakeup prevention requires the producer to lock the cv_mutex before notifying.

### 34.8 APSK constellation geometry (Phase 4)

DVB-S2 compliant 16-APSK (2 rings: 4+12, γ=2.85) and 32-APSK (3 rings: 4+12+16) constellations were added to the `ModulatorSource`. Constellation points are normalized to unit average symbol energy. Bit mapping follows the DVB-S2 standard for inter-ring Gray-like labeling.

### 34.9 OFDM synthesis with in-house FFT (Phase 4)

`OfdmSource` implements multi-subcarrier OFDM generation using an in-house Cooley-Tukey radix-2 IFFT, avoiding external FFT library dependencies. Each OFDM symbol is generated by: mapping random QPSK symbols to active subcarrier bins, performing IFFT, and appending a configurable cyclic prefix (tail samples copied to front). The implementation includes bit-reversal permutation for correct DIT ordering.

### 34.10 Multi-emitter additive mixing (Phase 4)

The planner detects overlapping emitter windows on the same channel and groups them into `MixGroup` structures. The render worker renders each emitter independently into temporary buffers, then sums element-wise. The planner estimates peak sum and emits headroom warnings if the sum exceeds 1.0. The validator was relaxed to allow overlaps when `mixing` is set to `"additive"`.

### 34.11 Timed events and scheduling (Phase 5)

The scenario model now supports an `events` array with types: retune, gain_change, marker, and burst. The planner converts these into `TimelineEvent` objects with absolute timestamps. An `EventDispatcher` thread wakes at scheduled times and dispatches hardware commands through the HAL interface. Marker events log with wall-clock timestamps and are included in run reports.

### 34.12 Burst repeat scheduling (Phase 5)

Emitters support a `repeat` specification with count and interval_sec. The planner unrolls repeated emitters into N individual render instructions with computed start times. This enables radar pulse train and EW burst patterns without manual emitter duplication.

### 34.13 Planning diagnostics (Phase 5)

The planner computes a `ResourceEstimate` for each scenario including: estimated CPU load, peak memory usage, minimum inter-emitter gap, and timing feasibility. These diagnostics are displayed in dry-run output and included in JSON plan output, helping users assess whether their scenario is practical for the available hardware.
