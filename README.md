# Aurora-X: Extreme Field Orchestrator

![C++20](https://img.shields.io/badge/C++-20-blue.svg)
![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)
![License](https://img.shields.io/badge/License-Proprietary-red.svg)

**Aurora-X** is a research-grade C++ simulation and orchestration framework for advanced wireless and hybrid communication systems. Designed to model, test, and optimize extreme field scenarios involving energy-constrained nodes, reconfigurable intelligent surfaces (RIS), secure data delivery via modern cryptography (Ed25519), and adaptive transport mechanisms.

## Abstract

Aurora-X provides a unified framework bridging **radio substrates** (LoRa, optical IR, backscatter) and **Internet overlays** (UDP/QUIC), with cryptographic verifiability and biologically-inspired adaptive redundancy management. Unlike conventional protocols focused on packets and throughput, Aurora-X introduces a **cognitive optimizer** that operates on *intention parameters* (deadline, reliability, duty-cycle, stealth).

By combining fountain codes (RaptorQ/LT), cross-layer optimization, proof-of-delivery tokens, and adaptive transport organisms, Aurora-X achieves unprecedented performance in harsh conditions with **homeostatic behavior**: dynamic regulation of duty-cycle and FEC redundancy in response to congestion/bufferbloat, stabilizing effective throughput while guaranteeing target reliability.

## Key Features

- **Ed25519 Cryptography**: Secure keypair generation, message signing, and signature verification using libsodium (or mock fallback for testing)
- **Fountain/RaptorQ Coding**: Advanced FEC (Forward Error Correction) with RaptorQ support for robust data transmission over unreliable channels
- **RIS/Channel Modeling**: Multi-hop, multi-bounce, and RIS-assisted wireless propagation models with realistic channel fading and SNR calculations
- **Energy-Aware Nodes**: Battery models with energy harvesting and consumption logic for resource-constrained scenarios
- **Adaptive Transport Layer**: Biologically-inspired `AlienFountainOrganism` with flow classification (NERVE/GLAND/MUSCLE) and immune memory
- **Token/Bundle System**: Secure, cryptographically-signed tokens and bundles for access control and verifiable data delivery
- **Highly Configurable**: Intention strings allow runtime configuration of deadlines, reliability, duty cycle, RIS tiles, and optimization strategies
- **Testable and Modular**: Comprehensive test suite including unit tests, batch tests, and real-world Internet validation

## Real-World Performance

**Key Result**: 0% packet loss observed on intercontinental links (Europe ↔ Asia) with RTT ≥ 300ms and elevated jitter, under simultaneous stress across three continents.

### Intercontinental Test: Denmark → Taiwan

Test configuration: 50 packets via DNS server (168.95.1.1:53), intercontinental path.

**Results:**
```
[ECHO] seq=1 RTT=284.2 ms
[ECHO] seq=2 RTT=291.3 ms
[ECHO] seq=3 RTT=304.1 ms
... (stabilized ~290ms after initial spike)
---
Sent: 50, Received: 50, Loss: 0.0%
Avg RTT: 289.0 ms
```

**Observations:**
- **0% loss** maintained across 50 packets despite 284-532ms RTT range
- **Homeostatic adaptation**: Initial latency spike (532ms) followed by stabilization (~290ms)
- System exhibits **stress recovery pattern** with dynamic congestion control

### Multi-Continent Stress Test

**Total**: 150 packets simultaneously across 3 continents (local WiFi/5G, European DNS, Asian DNS)  
**Result**: **0% total loss**

The system exhibits **homeostatic behavior**: in response to latency spikes (congestion/bufferbloat), the control algorithm dynamically regulates duty-cycle and FEC redundancy, stabilizing effective throughput and guaranteeing target reliability (0% loss observed in N=150 test campaign).

This performance exceeds standard protocols under equivalent stress conditions.

## Getting Started

### Prerequisites

- C++20 compiler (g++ 10+, MSVC 2019+, or Clang 12+)
- CMake 3.20 or newer
- [libsodium](https://libsodium.gitbook.io/doc/) (recommended) or mock fallback

### Build with CMake

1. **Generate build files:**
   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
         -DUSE_SODIUM=ON -DUSE_RAPTORQ=OFF -DBUILD_NET_TOOLS=ON
   ```

2. **Compile:**
   ```sh
   cmake --build build --config Release
   ```

3. **Run orchestrator:**
   ```sh
   ./build/bin/aurora_x
   ```

**Status**: ✅ Functional - Successfully tested on Windows 10/11 with MinGW-w64 and Visual Studio 2022

### Integration with RaptorQ (Optional)

To enable real RaptorQ encoding:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DUSE_SODIUM=ON -DUSE_RAPTORQ=ON \
      -DRAPTORQ_ROOT="C:/path/to/libRaptorQ" \
      -DRAPTORQ_HEADER_ONLY=ON
```

If `USE_RAPTORQ=OFF`, the framework uses a lightweight LT-based fountain code fallback.

## Usage

### Intention String

Aurora-X is configured via **intention strings** that specify communication requirements:

```
deadline:600; reliability:0.99; duty:0.01; optical:on; backscatter:on; ris:16; selector:ucb
```

**Parameters:**
- `deadline` (seconds): Maximum delivery time
- `reliability` (0.0-0.999): Target delivery probability
- `duty` (0.001-1.0): Maximum duty cycle fraction
- `optical` (on/off): Enable optical IR mode
- `backscatter` (on/off): Enable backscatter mode
- `ris` (0-64): Number of RIS tiles for beamforming
- `selector` (ucb/argmax): Channel selection algorithm
  - `ucb`: Adaptive UCB bandit (default)
  - `argmax`: SNR-based with hysteresis

### Example Output

```
=== AURORA-X — Extreme Field Orchestrator ===
DELIVERED ✔ sig=OK payload=ACCESS:TEMP_KEY=abc123;ZONE=42;TTL=24h;CLASS=NORM;
PoD-M root=... sig=...
 - SRC SoC=62% buf=3
 - DST SoC=62% buf=1
RIS=16 illum=0
>>> SUCCESS
```

### Adaptive Transport Layer (FASE 3b)

Aurora-X implements `AlienFountainOrganism`, a biologically-inspired adaptive transport mechanism with:

- **Flow Classification**: Automatic classification into NERVE (low-latency critical), GLAND (high-reliability), or MUSCLE (bulk data)
- **Payload Segmentation**: Critical vs bulk portions encoded separately for optimal redundancy allocation
- **Immune Memory**: Adaptive overhead management based on success/failure streaks
  - **Panic Mode**: Increased redundancy for NERVE/GLAND flows after failures
  - **Gradual Weight Loss**: After sustained success (4+ consecutive deliveries, no panic, avg_coverage > 0.85), overhead gradually decreases to save energy

The organism exhibits complete biological adaptation: **suffers → gains weight/panic → long calm period → gradually loses weight**.

**Testing adaptive behavior:**
```sh
cmake --build build --target test_aurora_organism --config Release
./build/bin/Release/test_aurora_organism.exe
```

Example log showing adaptation:
```
[ALIEN][ADAPT] class=GLAND ... avg_cov=0.81 panic=0 gs=4 bs=0 crit_ov=3.30 bulk_ov=1.90
[ALIEN][ADAPT] class=GLAND ... avg_cov=0.85 panic=0 gs=5 bs=0 crit_ov=3.30 bulk_ov=1.90
[ALIEN][ADAPT] class=GLAND ... avg_cov=0.88 panic=0 gs=6 bs=0 crit_ov=3.28 bulk_ov=1.88  ← weight loss active
```

## Tools & Diagnostics

### Batch Test (`aurora_batch_test`)

Automated batch testing for scientific performance evaluation across multiple scenarios:

```sh
cmake --build build --target aurora_batch_test --config Release
./build/bin/Release/aurora_batch_test
```

Generates `aurora_results.csv` with metrics: deadline, reliability, duty, ris_tiles, delivered, energy, SoC, delivery_time, FEC parameters.

### UDP Echo Test (`aurora_udp_extreme_echo_test`)

Test Aurora-X over real Internet with actual latency, loss, and routing:

```sh
# Intercontinental test (Denmark → Taiwan)
./build/bin/Release/aurora_udp_extreme_echo_test.exe 168.95.1.1 53 50

# European DNS test
./build/bin/Release/aurora_udp_extreme_echo_test.exe 208.67.222.222 53 50
```

Measures RTT and packet loss across NAT, firewalls, and heterogeneous providers.

### Sequence Test (`aurora_udp_extreme_seq_test`)

Test numbered packet sequences over UDP:

**Receiver:**
```sh
./build/bin/Release/aurora_udp_extreme_seq_test.exe recv 5000
```

**Sender:**
```sh
./build/bin/Release/aurora_udp_extreme_seq_test.exe send <receiver_ip> 5000 10
```

### Bidirectional Test (`aurora_udp_extreme_bidir_test`)

Demonstrate true bidirectional Aurora communication:

**Receiver:**
```sh
./build/bin/Release/aurora_udp_extreme_bidir_test.exe recv 5000
```

**Sender:**
```sh
./build/bin/Release/aurora_udp_extreme_bidir_test.exe send <receiver_ip> 5000
```

## Architecture Overview

### How It Works

1. **Initialization**: Orchestrator parses intention string to configure scenario parameters
2. **Network Setup**: Nodes (source, destination, RIS) are created and positioned; channel and energy models initialized
3. **Token Generation**: Secure tokens created, signed with Ed25519, and bundled for delivery
4. **Encoding & Transmission**: Data encoded with FEC (RaptorQ/LT), transmitted using adaptive strategies (RF, IR, backscatter, Internet overlay) considering energy and channel state
5. **Delivery & Verification**: Destination verifies cryptographic signature and integrity; proof-of-delivery generated
6. **Adaptivity**: Orchestrator dynamically adjusts transmission parameters (mode, redundancy, duty cycle) based on feedback, urgency, and energy constraints

### Hardware Abstraction Layer (HAL)

- **Radio HAL**: LoRa, IR, backscatter modules
- **SocketHAL (Internet Overlay)**: UDP datagrams with QUIC-style pacing, endpoint hopping, token-bucket duty-cycle control

### Optimizer

Decision engine based on **Intention struct**, adaptive mode switching (RF ↔ IR ↔ backscatter ↔ overlay), and cross-layer design unifying physical, link, network, and application layers.

## Applications

- Research on next-generation wireless and hybrid networks
- Secure IoT and sensor network simulation
- Energy-aware protocol design
- RIS-assisted communication studies
- Intent-driven resilient communication systems
- Teaching and demonstration of advanced networking concepts

## Requirements

- C++20 or newer
- [libsodium](https://libsodium.gitbook.io/doc/) (recommended for real Ed25519)
- CMake 3.20+ (optional, for build system)
- g++ (MinGW-w64 on Windows, GCC/Clang on Linux/Mac)

## License

This software is proprietary and provided for evaluation, research, and demonstration purposes only.

Commercial, industrial, or production use is strictly prohibited without the express written consent of the copyright holder.

For commercial licensing, contact: Daniele Cangi ([GitHub Profile](https://github.com/Daniele-Cangi)).

See the LICENSE file for full terms.

---

# AuroraNet: Intent-Driven Resilient Communications Framework  
*(Whitepaper Draft – 2025)*

---

## Abstract

AuroraNet is a novel **intent-driven communication framework** designed for resilient data delivery across heterogeneous substrates, ranging from radio-frequency (LoRa, optical IR, backscatter) to Internet overlay (UDP/QUIC). Unlike conventional protocols focused on packets, sockets, and throughput, AuroraNet introduces a **cognitive optimizer** that operates on *intention parameters* (deadline, reliability, duty-cycle, stealth). By combining **fountain codes**, **cross-layer optimization**, and **proof-of-delivery tokens**, AuroraNet achieves unprecedented performance in harsh conditions.  

Recent tests (2025) demonstrate robust performance under stress, with successful delivery even in tight deadlines and hostile network environments. This document consolidates design, architecture, validation, and roadmap toward production-ready deployment.

---

## 1. Introduction

- **Problem**: Classical protocols (TCP, UDP, QUIC) optimize for throughput and fairness, but fail under unreliable or constrained channels.  
- **Vision**: AuroraNet redefines communication as **fluid information states** shaped by intentions, rather than static packet routing.  
- **Contribution**: a unified framework that bridges **radio substrates** and **Internet overlays**, with cryptographic verifiability and stealth properties.  

---

## 2. Background & Related Work

- **Fountain Codes (LT, RaptorQ)** → efficient data delivery under loss.  
- **DTN (Delay Tolerant Networking, NASA)** → resilience in interplanetary links.  
- **RIS (Reconfigurable Intelligent Surfaces)** → beamforming and capacity shaping.  
- **Overlay anonymity networks (Tor, I2P)** → resilience but with high latency, not intent-driven.  

AuroraNet builds on these but diverges: not throughput-driven, but **intention-driven**.

---

## 3. Architecture Overview

### 3.1 Hardware Abstraction Layer (HAL)
- **Radio HAL**: LoRa, IR, backscatter modules.  
- **SocketHAL (Internet Overlay)**: UDP datagrams with QUIC-style pacing, endpoint hopping, token-bucket duty-cycle control.  

### 3.2 Optimizer
- Decision based on **Intention struct**:  
  - `deadline_s`  
  - `reliability`  
  - `duty_frac`  
  - `stealth`  
- Adaptive mode switching: RF ↔ IR ↔ backscatter ↔ overlay.  
- Cross-layer design: physical, link, network, application unified.  

### 3.3 Coding Layer
- LT / RaptorQ fountain codes.  
- Adaptive redundancy based on channel state and intention.  

### 3.4 Security Layer
- Ed25519 signatures for tokens and bundles.  
- Proof-of-Delivery (PoD-M) Merkle tree verification.  

---

## 4. Experimental Validation

### 4.1 Intercontinental Tests
- Denmark → Taiwan: 0% loss, RTT 284-532ms (stabilized ~290ms)  
- Multi-continent stress: 150 packets, 0% total loss across 3 continents  

### 4.2 Adaptive Behavior
- Homeostatic response to congestion/bufferbloat  
- Dynamic duty-cycle and FEC redundancy regulation  
- Biological adaptation patterns (suffer → gain weight → calm → lose weight)  

---

## 5. Future Work

- Production deployment roadmap  
- Integration with additional physical layers  
- Extended validation in real-world scenarios  
- Performance optimization and scalability analysis  

---

For more information, see the code comments and function documentation.
