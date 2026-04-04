# Archerfish

CLI-first programmable vector signal generator for USRP.

## Prerequisites

- C++23 compiler (Clang 17+ or GCC 13+)
- CMake 3.25+
- pkg-config

**macOS (Homebrew):**
```bash
brew install fmt spdlog cli11 catch2 nlohmann-json libsamplerate
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install -y build-essential cmake pkg-config \
    libfmt-dev libspdlog-dev nlohmann-json3-dev libcatch2-3-dev \
    libsamplerate0-dev libcli11-dev
```

- UHD (optional — enables USRP hardware support)

## Quick Start

```bash
# Install dependencies (macOS)
brew install fmt spdlog cli11 catch2 nlohmann-json libsamplerate

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Or use CMake Presets:
# cmake --preset release && cmake --build --preset release

# Run tests
ctest --test-dir build --output-on-failure
```

## Usage

```bash
# Device operations
archerfish devices list
archerfish devices info --device usrp0

# Scenario workflow
archerfish scenario validate examples/future_start_cw.json
archerfish scenario plan examples/future_start_cw.json
archerfish scenario plan --json examples/chirp_burst.json
archerfish scenario dry-run examples/future_start_cw.json
archerfish scenario run examples/qpsk_burst.json

# Waveform generation
archerfish wave gen cw --rate 10e6 --duration 0.1 --amplitude 0.2 --output cw.cf32
archerfish wave gen chirp --rate 20e6 --duration 0.01 --f0 -1000000 --f1 1000000 --amplitude 0.3 --output chirp.cf32
archerfish wave gen qpsk --symbol-rate 1e6 --sps 8 --rrc 0.35 --duration 0.05 --amplitude 0.2 --output qpsk.cf32
archerfish wave gen pulse --rate 10e6 --duration 0.1 --pulse-width 0.01 --pri 0.05 --amplitude 0.5 --output pulse.cf32
archerfish wave gen ask --rate 10e6 --duration 0.1 --symbol-rate 1e6 --amplitude 0.3 --output ask.cf32
archerfish wave gen fsk --rate 10e6 --duration 0.1 --symbol-rate 1e6 --deviation 500e3 --amplitude 0.3 --output fsk.cf32
archerfish wave gen am --rate 10e6 --duration 0.1 --mod-freq 1e3 --mod-depth 0.5 --amplitude 0.3 --output am.cf32
archerfish wave gen fm --rate 10e6 --duration 0.1 --mod-freq 1e3 --deviation 50e3 --amplitude 0.3 --output fm.cf32
archerfish wave gen pm --rate 10e6 --duration 0.1 --mod-freq 1e3 --mod-index 0.5 --amplitude 0.3 --output pm.cf32
archerfish wave gen apsk16 --symbol-rate 1e6 --sps 8 --rrc 0.35 --duration 0.05 --amplitude 0.2 --output apsk16.cf32
archerfish wave gen apsk32 --symbol-rate 1e6 --sps 8 --rrc 0.35 --duration 0.05 --amplitude 0.15 --output apsk32.cf32
archerfish wave gen ofdm --rate 20e6 --duration 0.1 --amplitude 0.15 --output ofdm.cf32

# Waveform inspection
archerfish wave inspect cw.cf32
archerfish wave inspect cw.cf32 --json

# Diagnostics
archerfish doctor
archerfish version

# Schema and calibration
archerfish schema print
archerfish schema print --json
archerfish calib init --device usrp0
archerfish calib show --device usrp0

# Reports
archerfish report show runs/latest/report.json
```

## Scenario Format

Scenarios are JSON files describing what to transmit, where, and when.

```json
{
  "metadata": { "name": "cw_test" },
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
      "start_after_sec": 0.0,
      "duration_sec": 1.0,
      "waveform": {
        "type": "cw",
        "amplitude": 0.2
      }
    }
  ]
}
```

See `examples/` for complete scenarios covering all features:

| Category | Examples |
|----------|----------|
| Basic waveforms | CW, chirp, QPSK, APSK-16/32, OFDM, ASK, FSK |
| Analog modulation | AM, FM, PM (`am_fm_modulation.json`) |
| Pulse | Pulse train, repeated burst |
| Multi-emitter | Additive mixing, mixed scene (waveform_ref + impairments) |
| Events | Timed events (retune/gain), marker events |
| Multi-channel | `multi_channel.json` (sync groups, per-channel RF) |
| Replay mode | `replay_mode.json` (long-duration, zero-underrun) |
| Channel impairments | Phase noise, multipath, fading (Rayleigh/Rician), PA nonlinearity (Rapp/Saleh) |
| Dynamic events | Waveform switch, impairment enable/disable |
| Power control | `target_power.json` (target dBm via calibration) |

### Supported waveform types

| Type | Key parameters |
|------|---------------|
| `cw` | `amplitude` |
| `chirp` | `f0_hz`, `f1_hz`, `amplitude` |
| `noise` | `amplitude` |
| `bpsk`, `qpsk`, `8psk` | `symbol_rate`, `samples_per_symbol`, `rrc_alpha`, `amplitude` |
| `qam16`, `qam64` | `symbol_rate`, `samples_per_symbol`, `rrc_alpha`, `amplitude` |
| `multi_tone` | `tones` (array of `{frequency_hz, amplitude}`) |
| `file` | `path`, `loop` |
| `pulse` | `pulse_width_sec`, `pri_sec`, `amplitude` |
| `ask` | `symbol_rate`, `amplitude` |
| `fsk` | `symbol_rate`, `deviation_hz`, `amplitude` |
| `am` | `mod_freq_hz`, `mod_depth`, `amplitude` |
| `fm` | `mod_freq_hz`, `deviation_hz`, `amplitude` |
| `pm` | `mod_freq_hz`, `mod_index`, `amplitude` |
| `apsk16`, `apsk32` | `symbol_rate`, `samples_per_symbol`, `rrc_alpha`, `amplitude` |
| `ofdm` | `fft_size`, `cyclic_prefix_size`, `active_subcarriers`, `amplitude` |

### Waveform references

Define reusable waveforms in a top-level `waveforms` array and reference them by ID:

```json
{
  "waveforms": [
    { "id": "my_qpsk", "type": "qpsk", "symbol_rate": 1e6, "samples_per_symbol": 8, "rrc_alpha": 0.35, "amplitude": 0.2 }
  ],
  "emitters": [
    { "id": "e1", "device": "usrp0", "channel": 0, "start_after_sec": 0, "duration_sec": 0.1, "waveform_ref": "my_qpsk" }
  ]
}
```

### Impairments

Each emitter supports optional impairments:

```json
{
  "impairments": {
    "cfo_hz": 200.0,
    "phase_offset_rad": 0.1,
    "iq_gain_imbalance_db": 0.5,
    "iq_phase_imbalance_rad": 0.02,
    "dc_offset_i": 0.01,
    "dc_offset_q": -0.01,
    "awgn_power": 0.001,
    "amplitude_ripple_db": 0.1,
    "amplitude_ripple_freq_hz": 1000.0,
    "delay_sec": 0.0001,
    "burst_dropout_rate": 0.01,
    "burst_dropout_mean_burst_sec": 0.001
  }
}
```

### Events

Scenarios support timed control events:

```json
{
  "events": [
    { "target_device": "usrp0", "time_sec": 2.0, "type": "gain_change", "payload": { "gain_db": 25.0 } },
    { "target_device": "usrp0", "time_sec": 3.5, "type": "retune", "payload": { "freq_hz": 2400000000.0 } },
    { "target_device": "usrp0", "time_sec": 4.0, "type": "marker", "payload": { "label": "checkpoint" } }
  ]
}
```

### Burst Repeat

Emitters can repeat with configurable count and interval:

```json
{
  "id": "pulse_train",
  "device": "usrp0",
  "channel": 0,
  "start_after_sec": 1.0,
  "duration_sec": 0.001,
  "waveform": { "type": "pulse", "amplitude": 0.5 },
  "repeat": { "count": 10, "interval_sec": 0.1 }
}
```

### Additive Mixing

Overlapping emitters on the same channel can be mixed additively:

```json
{
  "id": "cw2",
  "device": "usrp0",
  "channel": 0,
  "start_after_sec": 0.5,
  "duration_sec": 1.0,
  "waveform": { "type": "cw", "amplitude": 0.15 },
  "mixing": "additive"
}
```

## Project Structure

```
archerfish/
├── cli/                CLI subcommand definitions
├── cmake/              CMake modules (FindSampleRate, CompilerWarnings)
├── core/
│   ├── common/         Shared types, error handling
│   ├── dsp/            Waveform generators and DSP blocks
│   ├── hal/            Hardware abstraction (UHD wrapper)
│   ├── impairments/    Channel impairment models
│   ├── reporting/      Metrics and report generation
│   ├── runtime/        Execution engine, threading
│   └── scheduler/      Scenario parser, validator, planner
├── docs/               Design and architecture docs
├── examples/           Example scenario files
├── schemas/            JSON schema definitions
├── src/                Main executable entry point
└── tests/
    ├── unit/           Unit tests
    └── integration/    Integration + E2E tests
```

## Architecture

The pipeline flows through distinct phases:

```
Parse → Validate → Plan → Prepare → Execute → Report
```

**Threading model** (single-device MVP):
- **Main thread** — CLI entry, lifecycle control
- **Render thread** — generates waveform blocks into bounded SPSC queue
- **TX worker thread** — reads blocks from queue, streams to device
- **Event dispatcher** — fires timed control events

For multi-emitter scenarios on one channel:
- Non-overlapping emitters execute sequentially
- Overlapping emitters with `mixing: "additive"` are rendered independently and summed element-wise
- Timed events (retune, gain change, markers) are dispatched by a dedicated event thread

## Testing

```bash
# Full test suite (137 tests)
ctest --test-dir build --output-on-failure

# Individual test binaries
./build/test_cw_source
./build/test_e2e_pipeline
```

## Installing

```bash
cmake --install build --prefix /usr/local
```

See [docs/building.md](docs/building.md) for detailed build instructions, CMake presets, sanitizer builds, and troubleshooting.

## License

Proprietary — see [LICENSE](LICENSE) for details. This is **not** an open-source license.
