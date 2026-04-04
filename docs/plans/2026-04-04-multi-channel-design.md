# Multi-Channel TX Design Document

**Date**: 2026-04-04
**Status**: Approved
**Depends on**: Audit fixes H1, L5 (completed)

---

## 1. Overview

Archerfish currently supports single-channel TX per device. This document defines how multi-channel TX is added — allowing a single USRP device to transmit on multiple RF channels simultaneously with independent waveforms, gain, and frequency settings.

---

## 2. Design Decisions

### D1: Schema representation

**Decision**: Extend the existing device-level model with an explicit `channels` array.

Both modes are backward compatible:
- **Simple mode** (existing): `devices[].channel` field + `devices[].rf` — single channel per device entry
- **Explicit mode** (new): Top-level `channels[]` array with `{id, device, index, rf}` — multiple channels on one device

When `channels[]` is present, it takes precedence. When absent, the parser normalizes `devices[]` entries into implicit channel definitions (current behavior).

### D2: Queue model

**Decision**: One SPSC queue per channel.

Rationale:
- Simpler to reason about — each channel has its own render→TX pipeline
- Natural parallelism — channels don't block each other
- Memory overhead is acceptable (64 blocks × 32K samples × 2 channels = ~32MB)
- Shared queue with channel tagging would require additional synchronization and complex scheduling

### D3: Render threading

**Decision**: One render thread + one TX thread per channel (N render + N TX threads for N channels).

Rationale:
- Each channel may have different sample rates and waveform complexity
- Multiplexed single thread would add scheduling complexity
- For typical 2-channel scenarios, 4 threads (2 render + 2 TX) is well within modern CPU budgets
- The existing single-channel architecture maps naturally to per-channel worker pairs

### D4: Different sample rates across channels

**Decision**: Supported — each channel has independent `rf.rate_sps`.

The planner creates per-channel `ChannelPlan` with independent sample rates. The HAL already accepts `set_sample_rate(channel, sps)`. The runtime creates separate SPSC queues with block sizes computed per-channel.

Constraint: Channels on the same physical device share the same master clock. UHD may reject incompatible rate combinations. The validator warns (not errors) when channels on the same device have different rates.

### D5: Sync groups

**Decision**: Declared as metadata, enforced at runtime.

```json
{
  "sync_groups": [
    { "id": "beam_pair", "channels": ["ch0", "ch1"], "mode": "coherent" }
  ]
}
```

Coherent mode requires:
- Same device
- Shared clock/time source
- Same sample rate

The validator checks these constraints. The runtime logs phase relationship metadata but does not enforce phase coherence (that requires hardware-specific calibration beyond software scope).

---

## 3. Data Model Changes

### 3.1 New types in `scenario.hpp`

```cpp
struct ChannelDef {
    std::string id;           // e.g., "ch0"
    std::string device;       // references DeviceDef.id
    uint32_t index{0};        // hardware channel index
    RfSettings rf;            // per-channel RF settings
};

struct SyncGroup {
    std::string id;
    std::vector<std::string> channels;  // references ChannelDef.id
    std::string mode;                    // "coherent" | "independent"
};

struct Scenario {
    // ... existing fields ...
    std::vector<ChannelDef> channel_defs;  // explicit channel definitions
    std::vector<SyncGroup> sync_groups;    // sync group declarations
};
```

### 3.2 New types in `plan.hpp`

```cpp
struct ChannelPlan {
    std::string channel_id;
    uint32_t channel_index;
    RfSettings rf;
    std::vector<RenderInstruction> render_instructions;
    std::vector<TimelineEvent> events;
    std::vector<MixGroup> mix_groups;
};

struct Plan {
    // ... existing fields ...
    std::vector<ChannelPlan> channel_plans;  // per-channel plans
};
```

### 3.3 Emitter-to-channel binding

Emitters reference channels in two ways:
1. By `(device, channel)` pair — existing behavior, resolves to implicit channel
2. By `channel_id` — new, references explicit ChannelDef.id

The parser normalizes both to `ChannelDef` objects. The validator confirms all emitter bindings resolve to valid channels.

### 3.4 Runtime model changes

```cpp
struct RuntimeConfig {
    size_t queue_capacity{64};
    size_t block_size{32768};
    // channel removed — now inferred from Plan
};

struct ChannelExecutor {
    uint32_t channel_index;
    std::unique_ptr<SampleQueue> queue;
    std::vector<RenderJob> render_jobs;
    std::vector<MixGroupJob> mix_group_jobs;
};

class Runtime {
    // ... existing ...
    std::vector<ChannelExecutor> channel_executors_;
};
```

---

## 4. Schema Changes (`scenario.schema.json`)

### 4.1 New top-level `channels` property

```json
{
  "channels": {
    "type": "array",
    "description": "Explicit channel definitions (takes precedence over device-level channel field)",
    "items": {
      "type": "object",
      "required": ["id", "device", "index", "rf"],
      "additionalProperties": false,
      "properties": {
        "id": { "type": "string", "minLength": 1 },
        "device": { "type": "string", "minLength": 1 },
        "index": { "type": "integer", "minimum": 0 },
        "rf": { "$ref": "#/$defs/rf_settings" }
      }
    }
  },
  "sync_groups": {
    "type": "array",
    "items": {
      "type": "object",
      "required": ["id", "channels", "mode"],
      "additionalProperties": false,
      "properties": {
        "id": { "type": "string" },
        "channels": { "type": "array", "items": { "type": "string" } },
        "mode": { "type": "string", "enum": ["coherent", "independent"] }
      }
    }
  }
}
```

### 4.2 Emitter gains `channel_id` field

```json
{
  "emitter": {
    "properties": {
      "channel_id": {
        "type": "string",
        "description": "Reference to explicit channel definition by ID"
      }
    }
  }
}
```

---

## 5. Parser Changes

### 5.1 Normalization logic

1. If `channels[]` exists: parse each into `ChannelDef`, validate uniqueness of IDs
2. If no `channels[]`: create one `ChannelDef` per `devices[]` entry using `(device_id, channel_field)`
3. For each emitter: resolve `channel_id` or `(device, channel)` to a `ChannelDef`
4. After normalization, `Scenario.channel_defs` is the single source of truth

---

## 6. Planner Changes

### 6.1 Per-channel planning

The planner iterates over channels and creates a `ChannelPlan` for each:
- Each channel has its own set of render instructions (filtered by channel binding)
- Each channel has its own timeline events
- Each channel has its own mix groups
- Resource estimate sums across channels

### 6.2 Cross-channel timing

For channels on the same device:
- All channels share a common time epoch (set by `sync_time_now()`)
- Emitters targeting different channels have independent timing
- The runtime ensures coordinated start (all channels begin at the same wall-clock time)

---

## 7. HAL Changes

### 7.1 Multi-channel TX streamer management

`IHalDevice` already accepts `uint32_t channel` on all methods. The existing `UhdDevice` maintains a per-channel streamer map. No interface changes needed.

### 7.2 Multi-channel capability query

`DeviceCapabilities` already has `num_channels`. The validator will use this to reject scenarios requesting non-existent channels.

---

## 8. Runtime Changes

### 8.1 ChannelExecutor

A new helper class (or struct) that encapsulates per-channel state:
- One SPSC queue
- One set of render jobs and mix group jobs
- Per-channel metrics

### 8.2 Execution flow

1. **Prepare**: Configure all channel RF settings (existing `prepare()` already iterates `plan.channels`)
2. **Arm**: Create a `ChannelExecutor` for each channel, populate jobs
3. **Run**:
   a. Start TX on all channels: `device_->start_tx(ch)` for each channel
   b. For each channel, launch render + TX worker pair (may run concurrently across channels)
   c. Wait for all workers to complete
   d. Drain and stop all channels

### 8.3 Coordinated start

All channels start TX at the same wall-clock time. The runtime:
1. Calls `device_->start_tx(ch)` for all channels
2. Uses `device_->sync_time_now()` to establish epoch
3. Workers begin sending samples simultaneously

### 8.4 Per-channel metrics

`RunMetrics` extended to include per-channel breakdown:
```cpp
struct ChannelMetrics {
    uint32_t channel_index;
    size_t samples_sent{0};
    size_t blocks_sent{0};
    size_t underruns{0};
    double active_duration_sec{0.0};
};

struct RunMetrics {
    // ... existing aggregate fields ...
    std::vector<ChannelMetrics> per_channel;
};
```

---

## 9. Event Handling

Events are dispatched per-channel. The existing `EventDispatcher` is shared across all channels — events include their target channel in the payload. For multi-channel, retune/gain events specify the channel index.

---

## 10. Backward Compatibility

- Single-channel scenarios work unchanged — no `channels[]`, no `sync_groups[]`
- The parser normalizes single-channel into the same `ChannelDef` format
- The planner creates a single `ChannelPlan`
- The runtime creates a single `ChannelExecutor`
- All 60 existing tests pass without modification

---

## 11. Test Plan

| Test | Validates |
|------|-----------|
| `test_multi_channel_parser.cpp` | Explicit channels parse, backward compat, channel ref resolution |
| `test_multi_channel_planner.cpp` | Per-channel plans, cross-channel timing, resource estimates |
| `test_multi_channel_hal.cpp` | Multi-channel capability query, per-channel RF config |
| `test_multi_channel_run.cpp` | Integration: two-channel CW scenario, per-channel metrics |
| `test_sync_group.cpp` | Sync group validation, coherent mode constraints |

Target: +80 tests (from 60 to 140+)
