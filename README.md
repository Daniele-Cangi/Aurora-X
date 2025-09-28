![aurora_banner_svg](https://github.com/user-attachments/assets/50696678-41e5-4f89-98e7-c25780a898b3)# Aurora Extreme Orchestrator

Aurora Extreme Orchestrator (AURORA-X) is a research-grade C++ simulation and orchestration framework for advanced wireless and hybrid communication systems. It is designed to model, test, and optimize extreme field scenarios involving energy-constrained nodes, reconfigurable intelligent surfaces (RIS), and secure data delivery using modern cryptography (Ed25519).

## Key Features

- **Ed25519 Cryptography**: Secure keypair generation, message signing, and signature verification using libsodium (or a mock fallback).
- **Fountain/RaptorQ Coding**: Simulated FEC (Forward Error Correction) for robust data transmission over unreliable channels.
- **RIS/Channel Modeling**: Models multi-hop, multi-bounce, and RIS-assisted wireless propagation, including realistic channel fading and SNR calculations.
- **Energy-Aware Nodes**: Each node has a battery model, energy harvesting, and consumption logic.
- **Adaptive Orchestration**: The system adapts transmission strategies (mode, redundancy, duty cycle, etc.) based on scenario parameters and feedback.
- **Token/Bundle System**: Secure, signed tokens and bundles for access control and data delivery.
- **Highly Configurable**: Intention strings allow you to set deadlines, reliability, duty cycle, RIS tiles, and more for each simulation.
- **Testable and Modular**: Includes unit tests for cryptography and can be extended for new protocols or physical layers.

## How It Works

1. **Initialization**: The orchestrator parses an "intention" string (e.g., `deadline:600; reliability:0.99; duty:0.01; ris:16;`) to configure the scenario.
2. **Network Setup**: Nodes (source, destination, RIS) are created and positioned. The channel and energy models are initialized.
3. **Token Generation**: Secure tokens are created, signed, and bundled for delivery.
4. **Encoding & Transmission**: Data is encoded with FEC and transmitted using adaptive strategies (RF, IR, backscatter, etc.), considering energy and channel state.
5. **Delivery & Verification**: The destination verifies the signature and integrity of received data. Proof-of-delivery is generated and logged.
6. **Adaptivity**: The orchestrator dynamically adjusts transmission parameters based on feedback, urgency, and energy constraints.

## Example Output
```
=== AURORA-X — Extreme Field Orchestrator ===
DELIVERED ✔ sig=OK payload=ACCESS:TEMP_KEY=abc123;ZONE=42;TTL=24h;CLASS=NORM;
PoD-M root=... sig=...
 - SRC SoC=62% buf=3
 - DST SoC=62% buf=1
RIS=16 illum=0
>>> SUCCESS
```
**Status**: ✅ Functional - Successfully tested on Windows 10/11 with MinGW-w64

## Usage

1. **Build** (requires g++ and libsodium):
   ```sh
   g++ -std=c++17 aurora_x.cpp -o aurora_x -lsodium
   ```
2. **Run**:
   ```sh
   ./aurora_x
   ```
3. **Modify Intention**: Edit the intention string in `aurora_x.cpp` to test different scenarios (e.g., change deadline, reliability, duty, ris, etc.).

## Applications
- Research on next-generation wireless and hybrid networks
- Secure IoT and sensor network simulation
- Energy-aware protocol design
- RIS-assisted communication studies
- Teaching and demonstration of advanced networking concepts

## Requirements
- C++17 or newer
- [libsodium](https://libsodium.gitbook.io/doc/)
- g++ (MinGW-w64 on Windows, GCC/Clang on Linux/Mac)


## License
This software is proprietary and provided for evaluation, research, and demonstration purposes only.
Commercial, industrial, or production use is strictly prohibited without the express written consent of the copyright holder.
For commercial licensing, contact: Daniele Cangi ([GitHub Profile](https://github.com/Daniele-Cangi)).
See the LICENSE file for full terms.

---

For more information, see the code comments and function documentation.
