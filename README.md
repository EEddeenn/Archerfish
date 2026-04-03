# Archerfish

CLI-first programmable vector signal generator for USRP.

## Prerequisites

- C++23 compiler (GCC 13+, Clang 17+)
- CMake 3.25+
- Conan 2
- UHD (optional — enables USRP hardware support)

## Quick Start

```bash
# Install dependencies
conan profile detect --force
conan install . --build=missing -s build_type=Release

# Configure and build
cmake --preset conan-release
cmake --build --preset conan-release

# Run tests
ctest --preset conan-release
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
archerfish scenario run examples/qpsk_burst.json

# Waveform generation
archerfish wave gen cw --rate 10e6 --duration 0.1 --amplitude 0.2 --output cw.cf32
archerfish wave gen chirp --rate 20e6 --duration 0.01 --f0 -1000000 --f1 1000000 --amplitude 0.3 --output chirp.cf32
archerfish wave gen qpsk --symbol-rate 1e6 --sps 8 --rrc 0.35 --duration 0.05 --amplitude 0.2 --output qpsk.cf32

# Waveform inspection
archerfish wave inspect cw.cf32
archerfish wave inspect cw.cf32 --json

# Diagnostics
archerfish doctor
archerfish version
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

See `examples/` for complete scenarios (CW, chirp, QPSK, mixed scene).

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
    "awgn_power": 0.001
  }
}
```

## Project Structure

```
archerfish/
├── cli/                CLI subcommand definitions
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

For multi-emitter scenarios on one channel, emitters are executed sequentially (non-overlapping windows required by validator).

## Testing

```bash
# Full test suite (37 tests)
ctest --preset conan-release --output-on-failure

# Individual test binaries
./build/Release/test_cw_source
./build/Release/test_e2e_pipeline
```

## Installing

```bash
cmake --install build/Release/generators/build/Release --prefix /usr/local
```

## License

MIT
