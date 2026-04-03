# Architecture

## Pipeline

```
Parse → Validate → Plan → Prepare → Execute → Report
```

Each phase is a distinct step with explicit inputs and outputs.

## Layers

All layers below are fully implemented in the current MVP.

```
┌─────────────────────────────────────┐
│            CLI (cli/)               │  CLI11 subcommands
├─────────────────────────────────────┤
│      Scheduler (core/scheduler/)    │  Parser, Validator, Planner
├─────────────────────────────────────┤
│       Runtime (core/runtime/)       │  State machine, threads, queue
├─────────────────────────────────────┤
│         DSP (core/dsp/)             │  Waveform generators, filters
├─────────────────────────────────────┤
│    Impairments (core/impairments/)  │  AWGN, CFO, IQ imbalance, DC
├─────────────────────────────────────┤
│    Reporting (core/reporting/)      │  Metrics, report, run directory
├─────────────────────────────────────┤
│          HAL (core/hal/)            │  IHalDevice interface + stub
└─────────────────────────────────────┘
```

## Threading

| Thread | Role |
|--------|------|
| Main | CLI entry, lifecycle, result collection |
| Render | Generates waveform blocks into SPSC queue |
| TX Worker | Reads blocks from queue, sends to device |
| Event Dispatcher | Fires timed control events |

## State Machine

```
Created → Validated → Planned → Prepared → Armed → Running → Completed
                                                       ↘ Failed
                                                       ↘ Aborted
```

Terminal states (Completed, Aborted, Failed) allow no further transitions.

## Key Data Flows

### Scenario → Plan

```
Scenario JSON
  → Parser: Scenario struct
  → Validator: ValidationResult (errors + warnings)
  → Planner: Plan (channels, timeline, render instructions)
```

### Plan → TX (Render Pipeline)

```
Plan
  → Runtime::prepare()
      Configure device RF settings (freq, rate, gain)

  → Runtime::arm()
      Build RenderJob list from Plan render_instructions
      Each job binds an ISource + optional ImpairmentChain to a channel

  → Runtime::run()
      For each job:
        1. Create fresh SPSC queue + RenderWorker + TxWorker
        2. RenderWorker loop:
             ISource::render_block() → SampleBlock → SPSC queue push
        3. TxWorker loop:
             SPSC queue pop → IHalDevice::send_samples()
```

Multi-emitter scenarios on one channel execute sequentially. The validator ensures non-overlapping time windows.

## DSP Source Types

All sources implement `ISource` with `configure() → prepare() → render_block() → reset()` lifecycle.

### Waveform Generators

| Source | Algorithm | Notes |
|--------|-----------|-------|
| CwSource | `A * exp(j*2π*f/sps * n)` | Supports frequency offset, continuous phase across blocks |
| ChirpSource | Linear FM from f0→f1 | Constant-amplitude frequency sweep |
| NoiseSource | AWGN via seeded `mt19937` | Reproducible with configurable seed |
| ModulatorSource | BPSK/QPSK/8-PSK/16-QAM/64-QAM | See Modulator Architecture below |
| MultiToneSource | Sum of N complex tones | Each tone has independent frequency and amplitude |
| FileSource | CF32 binary replay | Supports loop/finite modes, optional sidecar JSON metadata |
| PulseSource | Configurable pulse train | Pulse width, PRI, frequency, duty cycle |
| AskSource | Amplitude-shift keying | On-off keying with configurable symbol rate |
| FskSource | Frequency-shift keying | 2-tone FSK with configurable deviation |
| AmSource | AM modulation | Sinusoidal amplitude modulation with depth control |
| FmSource | FM modulation | Frequency modulation with configurable deviation |
| PmSource | PM modulation | Phase modulation with configurable modulation index |

### Processing Blocks

Free functions (not ISource implementations):

| Block | Operation |
|-------|-----------|
| Scaler | Complex multiply: `out[i] *= factor` |
| Summer | Element-wise add: `out[i] = a[i] + b[i]` with headroom check variant |
| ResamplerBlock | Sample rate conversion via libsamplerate (SRC_SINC_MEDIUM_QUALITY) |

## Modulator Architecture

`ModulatorSource` generates shaped symbol streams for BPSK through 64-QAM.

### Constellation Mapping

Gray-coded constellations built via `build_constellation()`. Algorithm follows liquid-dsp:
- Split symbol bits into I/Q axis indices
- Apply `gray_decode()` independently per axis
- Normalize to unit average symbol energy

Normalizations: QAM16 uses `1/√10`, QAM64 uses `1/√42`, QPSK uses `1/√2`.

### Pulse Shaping

Root raised cosine (RRC) filter via `RrcFilterDesign`:
- Configurable roll-off α (default 0.35), span (6 symbols), samples per symbol
- Overlap-save filtering preserves continuity across `render_block()` calls
- `filter_tail_` state carries the last `L-1` upsampled samples between batches

### Calibration

`prepare()` runs a 256-symbol calibration pass to compute peak/RMS ratios for metadata:
1. Generate 256 random symbols, upsample, convolve with RRC
2. Measure peak magnitude and RMS
3. Store `peak_to_rms_ratio_` and `rms_ratio_` for `report_metadata()`
4. RNG state is saved and restored so calibration does not affect the output stream

### Bit Generation

Seeded `mt19937` PRNG produces reproducible bit streams. Seed defaults to 42, configurable via JSON.

## Impairment Pipeline

### Interface

```cpp
class IImpairment {
    virtual void apply(complex<float>* data, size_t count) = 0;
    virtual string name() const = 0;
    virtual bool enabled() const = 0;
    virtual void set_enabled(bool v) = 0;
};
```

### Chain

`ImpairmentChain` holds an ordered vector of `unique_ptr<IImpairment>`. `apply()` iterates sequentially, calling each enabled impairment. Supports runtime toggle per impairment.

### Implemented Impairments

| Impairment | Class | Effect |
|------------|-------|--------|
| AWGN | `AwgnImpairment` | Adds complex Gaussian noise (seeded `mt19937`) |
| Carrier Frequency Offset | `CfoImpairment` | Applies `exp(j*2π*cfo/sample_rate * n)` rotation |
| Phase Offset | `PhaseOffsetImpairment` | Constant `exp(j*φ)` rotation |
| IQ Imbalance | `IqImbalanceImpairment` | Gain imbalance (dB) + phase imbalance (rad) |
| DC Offset | `DcOffsetImpairment` | Adds constant `(dc_i, dc_q)` to every sample |
| Amplitude Ripple | `AmplitudeRippleImpairment` | Sinusoidal gain variation at configurable frequency and depth |
| Delay | `DelayImpairment` | Shifts samples by N samples with zero-fill buffer |
| Burst Dropout | `BurstDropoutImpairment` | Randomly drops bursts of samples with configurable rate and duration |

## CLI Commands

All commands are CLI11 subcommands under the `archerfish` binary.

| Command | Description |
|---------|-------------|
| `archerfish devices list` | List detected devices |
| `archerfish devices info --device <id>` | Show device details |
| `archerfish scenario validate <file>` | Validate scenario JSON |
| `archerfish scenario plan <file>` | Generate execution plan |
| `archerfish scenario run <file>` | Execute scenario end to end |
| `archerfish scenario dry-run <file>` | Dry-run scenario with ASCII timeline |
| `archerfish wave gen cw [opts]` | Generate CW waveform to CF32 file |
| `archerfish wave gen chirp [opts]` | Generate chirp waveform to CF32 file |
| `archerfish wave gen qpsk [opts]` | Generate QPSK waveform to CF32 file |
| `archerfish wave gen pulse [opts]` | Generate pulse train waveform to CF32 file |
| `archerfish wave gen ask [opts]` | Generate ASK waveform to CF32 file |
| `archerfish wave gen fsk [opts]` | Generate FSK waveform to CF32 file |
| `archerfish wave gen am [opts]` | Generate AM waveform to CF32 file |
| `archerfish wave gen fm [opts]` | Generate FM waveform to CF32 file |
| `archerfish wave gen pm [opts]` | Generate PM waveform to CF32 file |
| `archerfish wave inspect <file>` | Print waveform metadata |
| `archerfish report show <file>` | Display execution report |
| `archerfish doctor` | Run diagnostic checks |
| `archerfish version` | Print version |

## Module Dependencies

Verified against CMakeLists.txt link targets:

```
cli → common, scheduler, runtime, hal, dsp, reporting
 runtime → common, hal, dsp, scheduler, impairments
 scheduler → common
 dsp → common
 impairments → common
 reporting → common, scheduler
 hal → common
 ```

> Note: `archerfish_cli` additionally links `nlohmann_json`, and `fmt::fmt`/`spdlog::spdlog`/`CLI11::CLI11`.

`common` is the leaf dependency. No module depends upward.
