# Architecture

## Pipeline

```
Parse → Validate → Plan → Prepare → Execute → Report
```

Each phase is a distinct step with explicit inputs and outputs.

## Layers

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

### Plan → TX

```
Plan
  → Runtime::prepare(): configure device RF settings
  → Runtime::arm(): build RenderJob list from render instructions
  → Runtime::run(): for each job, create fresh SPSC queue + RenderWorker + TxWorker
    → RenderWorker: ISource::render_block() → SampleBlock → SPSC queue
    → TxWorker: SPSC queue → IHalDevice::send_samples()
```

## Module Dependencies

```
cli → scheduler, runtime, hal
runtime → dsp, hal, scheduler
scheduler → common
dsp → common
impairments → common
reporting → common, scheduler
hal → common
```

`common` is the leaf dependency — no module depends upward.
