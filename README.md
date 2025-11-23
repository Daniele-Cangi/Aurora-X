# Aurora-X – Bio-Inspired Extreme Overlay Engine

Aurora-X is a research-grade C++ engine for **extreme networks**: energy-constrained nodes, harsh wireless channels, RIS-assisted links, and secure delivery with Ed25519.

In the current line of work, Aurora-X integrates a **bio-inspired, self-adapting FEC organism** with a full **closed-loop safety & optimizer stack**:

> **Channel → AlienFountainOrganism → FlowHealth → SafetyMonitor → Optimizer Mode → Engine → Channel**

This repo is not a generic UDP demo: it is a sandbox for **adaptive, immune-like transport** under stress.

---

## 1. Core Ideas

### 1.1 AlienFountainOrganism (Adaptive FEC Core)

Aurora-X replaces fixed FEC parameters (and the old RaptorQ integration) with an internal, LT/fountain-based engine called **`AlienFountainOrganism`**:

- Splits each payload into:
  - **critical segment** (headers / vital bytes),
  - **bulk segment** (rest of the payload).
- Maintains separate encoders/decoders for:
  - critical symbols (`enc_crit`),
  - bulk symbols (`enc_bulk`).
- Dynamically adjusts redundancy via **overhead factors**:
  - `crit_overhead`, `bulk_overhead` ∈ [1.0, 4.0],
  - with **genetic baselines** per traffic class.

Traffic is classified biologically into:

- **NERVE** – latency-critical (short deadlines, high reliability),
- **GLAND** – high-reliability control/state flows,
- **MUSCLE** – bulk / elastic traffic.

Each (FlowClass, priority) pair has its own **FlowState** with:

- adaptive overhead (crit/bulk),
- coverage EWMA,
- success/fail counters,
- good/bad streaks,
- a `panic_boost` field for acute reactions.

### 1.2 Immunological Adaptation & Panic Mode

On every decode (`integrate()`):

- Compute:
  - `coverage` ∈ [0, 1],
  - `symbols_used`, `total_symbols_seen`,
  - boolean `delivered`.
- Update FlowState via **immunological rules**:
  - **Failure (`delivered == false`)**:
    - increase `crit_overhead` and `bulk_overhead` (fast `alpha_up`),
    - for NERVE/GLAND: trigger **panic mode**:
      - set `panic_boost` for the next N spawns,
      - temporarily multiply redundancy (e.g. 2× on critical).
  - **Success with excess redundancy**:
    - if we used "too many" symbols, slowly **slim down**:
      - decrease overhead by `alpha_down`,
      - never below genetic baselines.

Panic mode behaves like an **acute inflammatory response**: violent, short-lived boosts of redundancy to survive a shock.

---

## 2. Closed-Loop Safety & Optimizer

Above the organism, Aurora-X aggregates and reacts to flow-level health.

### 2.1 FlowHealth (NERVE / GLAND / MUSCLE)

Per class, Aurora-X builds a **FlowHealth** summary:

- `ewma_cov` – exponentially weighted coverage,
- `ewma_fail_rate` – long-term failure rate,
- `good_streak` / `bad_streak`,
- `recent_good_ratio` – short-term success ratio (EWMA),
- small window counters.

This is the **"vital signs" vector** for each organ-class.

### 2.2 AuroraSafetyMonitor (SafetyState)

`AuroraSafetyMonitor` consumes FlowHealth and keeps a discrete **`SafetyState`**:

- `HEALTHY`
- `DEGRADED`
- `CRITICAL`

Key behaviours:

- **Shock detection**:
  - if GLAND fail rate & bad streak explode → enter **CRITICAL** fast.
- **Hysteresis & recovery**:
  - separate thresholds for enter/exit CRITICAL,
  - requires *sustained* good evidence to climb back:
    - GLAND recent_good_ratio high,
    - ewma_fail_rate low,
    - long good streak, zero recent failures,
    - maintained for a configurable number of steps.
- Logs transitions:
  - `[SAFETY][STATE] ...`
  - `[SAFETY][RECOVERY] state: CRITICAL→DEGRADED`
  - `[SAFETY][RECOVERY] state: DEGRADED→HEALTHY`

### 2.3 Optimizer Mode & Genotypes

The **Optimizer** maps SafetyState + FlowHealth to a **Mode**:

- `CONSERVATIVE` – high redundancy, cautious use of resources.
- `NORMAL` – baseline.
- `AGGRESSIVE` – minimal redundancy, exploratory.

Transitions are logged with `[OPT][MODE]` / `[OPT][RECOVERY]`.

Internally the system can also emit **genotype hints** per flow class:

- `BASELINE`, `HYPERVIGILANT`, `STOIC`, `EXPERIMENTAL`

to bias how aggressively each class should defend or experiment. In the current configuration, **EXPERIMENTAL/STOIC** are only allowed in genuinely healthy regimes (low failure, AGGRESSIVE mode, zero recent bad streaks).

---

## 3. Repository Layout (Main Components)

- `aurora_x.cpp`  
  Main orchestrator and simulation harness:
  - parses intentions,
  - drives `Engine`,
  - manages **FlowHealth**, SafetyMonitor, Optimizer,
  - includes **three-phase shock/recovery scenario**.

- `aurora_extreme.hpp`  
  Core types and optimizer:
  - `FlowClass`, `FlowHealth`,
  - `Mode` and update logic,
  - duty, redundancy, SoC, channel/routing abstractions.

- `aurora_organism.hpp`  
  **AlienFountainOrganism**:
  - flow profiling (NERVE / GLAND / MUSCLE),
  - critical/bulk segmentation,
  - adaptive overhead + panic mode,
  - immunological update rules.

- `src/core/AuroraSafetyMonitor.hpp`  
  Safety supervisor:
  - `SafetyState` + hysteresis,
  - shock detection and recovery.

- `aurora_batch_test.cpp`  
  Batch experiments:
  - sweep intentions / parameters,
  - write `aurora_results.csv` for analysis.

- `analyze_health_log.py`  
  Python helper to analyse **health logs** (CSV) generated by the Engine:
  - timeline of SafetyState / Mode,
  - flow-class health metrics.

- `aurora_udp_*_test.cpp`  
  Real-world UDP demos (local LAN and Internet).

---

## 4. Build & Run

### 4.1 Minimal Build (Single File)

On a system with `g++` and (optionally) `libsodium`:

```sh
g++ -std=c++20 -O3 -pthread aurora_x.cpp -o aurora_x
./aurora_x
```

The current default path uses the internal AlienFountainOrganism FEC and does not require RaptorQ.

### 4.2 CMake (Recommended)

Example CMake invocation:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DUSE_SODIUM=ON \
      -DUSE_RAPTORQ=OFF \
      -DBUILD_FIELD=OFF \
      -DBUILD_UDP_DEMOS=ON
cmake --build build --config Release --target aurora_x
```

- `USE_SODIUM=ON` – enable real Ed25519 via libsodium.
- `USE_RAPTORQ=OFF` – the current engine relies on AlienFountainOrganism; RaptorQ integration is legacy/experimental.
- `BUILD_UDP_DEMOS=ON` – build the UDP tools (aurora_udp_demo, aurora_udp_extreme_*_test, …).

Run:

```sh
./build/bin/aurora_x
```

---

## 5. Automatic Batch Tests

### 5.1 Batch Test (Parameter Sweep)

`aurora_batch_test.cpp` runs a series of simulations and writes the results to CSV:

```sh
g++ -std=c++20 -O3 -pthread aurora_batch_test.cpp -o aurora_batch_test
./aurora_batch_test
```

This generates:

- `aurora_results.csv` – per-run metrics:
  - deadlines, target reliability, duty,
  - RIS tiles,
  - delivered / not delivered,
  - energy / SoC,
  - delivery time, etc.

You can analyse it with Python, pandas, Excel, R, etc.

---

## 6. Shock–Recovery Scenario & Health Logs

### 6.1 Three-Phase Scenario

`aurora_x.cpp` includes a three-phase stress scenario:

**Phase A – Shock**
- GLAND success ≈ 10% (brutal failure),
- NERVE / MUSCLE ≈ 90%.

**Phase B – Recovery**
- GLAND success ≈ 100%,
- NERVE / MUSCLE ≈ 95–98%.

**Phase C – Stable Healthy Regime**
- All classes ≈ 99% success.

Internally:
- Engine adjusts crit_overhead / bulk_overhead,
- FlowHealth encodes long/short-term trauma vs healing,
- SafetyMonitor updates SafetyState,
- Optimizer updates Mode.

### 6.2 Health Logger (CSV)

During long runs, Aurora-X emits a CSV health timeline, e.g. `aurora_health_log.csv`:

**Columns** (per step, per FlowClass):
- `step`
- `time_s`
- `flow_class` (NERVE / GLAND / MUSCLE)
- `ewma_cov`
- `ewma_fail`
- `good_streak`
- `bad_streak`
- `safety_state`
- `opt_mode`

Use the Python helper to inspect it:

```sh
python analyze_health_log.py aurora_health_log.csv
```

It reports, for example:
- how many steps spent in each SafetyState,
- how often the optimizer switched mode,
- whether GLAND successfully recovered from a CRITICAL shock.

---

## 7. Real-World UDP Demos

Aurora-X includes several tools to project the engine on real networks.

### 7.1 UDP Echo over the Internet

`aurora_udp_extreme_echo_test.cpp` sends Aurora packets to a UDP echo server and measures RTT / loss:

```sh
g++ aurora_udp_extreme_echo_test.cpp -o aurora_udp_extreme_echo_test -lws2_32
.\aurora_udp_extreme_echo_test.exe 18.133.69.55 7 10
```

Typical output:

```
[ECHO] seq=1 RTT=98.2 ms
[ECHO] seq=2 RTT=101.5 ms
...
---
Sent: 10, Received: 9, Loss: 10.0%
Avg RTT: 102.3 ms
```

You can also use DNS servers as "UDP echo-like" endpoints (e.g. 208.67.222.222:53, 8.8.8.8:53, etc.).

### 7.2 DNS-Based Global Tests

Example from Denmark to Taiwan DNS (168.95.1.1:53):

```sh
.\aurora_udp_extreme_echo_test.exe 168.95.1.1 53 10
```

Example measured output:

```
[ECHO] seq=1 RTT=284.2 ms
...
Sent: 10, Received: 10, Loss: 0.0%
Avg RTT: 289.0 ms
```

This demonstrates:
- Aurora-X traffic crossing NAT, firewall, ISPs,
- realistic RTTs for intercontinental paths.

### 7.3 Local Sequence & Bidirectional Tests

- `aurora_udp_extreme_seq_test.cpp` – send/receive numbered Aurora packets across LAN.
- `aurora_udp_extreme_bidir_test.cpp` – bidirectional state exchange between two nodes:

```sh
# Receiver
.\aurora_udp_extreme_bidir_test.exe recv 5000

# Sender
.\aurora_udp_extreme_bidir_test.exe send 127.0.0.1 5000
```

These demos show that Aurora states and telemetry can be serialized, transported, and updated across real networks.

---

## 8. Applications

- Research on adaptive, immune-like FEC and closed-loop safety controllers.
- Simulation of energy-aware, RIS-assisted wireless networks.
- Prototyping of secure IoT / sensor networks under extreme conditions.
- Teaching advanced topics:
  - hybrid systems,
  - supervisory control,
  - error-control coding + safety logic integration.

---

## 9. License

This software is proprietary and provided for evaluation, research, and demonstration purposes only.

Commercial, industrial, or production use is strictly prohibited without explicit written consent from the copyright holder.

For commercial licensing, contact: Daniele Cangi (GitHub Profile).

See LICENSE for full terms.

---

## 10. Relation to Prior Work

Aurora-X sits at the intersection of several research domains:

- **Adaptive FEC**: Unlike fixed-rate codes (e.g., Reed-Solomon, LDPC), Aurora-X uses fountain codes with dynamic redundancy that adapts to channel conditions and traffic class requirements.

- **Artificial Immune Systems (AIS)**: The panic mode and immunological adaptation rules draw inspiration from biological immune responses—rapid, temporary boosts in defense when threats are detected.

- **Supervisory Control Theory**: The SafetyMonitor implements a discrete-state supervisor with hysteresis, ensuring stable transitions between operational modes (HEALTHY/DEGRADED/CRITICAL).

- **Network Coding & Fountain Codes**: Builds on LT codes and fountain coding principles, but extends them with class-aware segmentation (critical vs. bulk) and adaptive overhead factors.

- **Extreme Networks Research**: Targets scenarios where traditional protocols fail—energy-constrained nodes, intermittent connectivity, and harsh wireless environments typical of IoT, sensor networks, and space communications.
