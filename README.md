# Harry Potter Wand Duel - RP2040 Embedded Edge-AI System

An interactive, dual-wand dueling system built on the Raspberry Pi RP2040 microcontroller. The wand tracks physical spell-casting gestures in real time using on-device machine learning (TinyML), transmits spell data wirelessly over 38 kHz modulated infrared, renders real-time combat status and animations on a 16x16 RGB LED matrix, and delivers tactile feedback via an onboard haptic motor driver.

Designed and developed for **ECE 362: Microprocessor Systems and Interfacing** at Purdue University.

---

## System Architecture

```
                      +-------------------------------------------------------+
                      |                 Raspberry Pi RP2040                   |
                      |                                                       |
                      |  +-------------------------------------------------+  |
                      |  | Core 0: Real-Time Engine & Peripherals          |  |
                      |  |                                                 |  |
                      |  |  * 74 Hz IMU Poller (BNO085 over I2C0)          |  |
                      |  |  * 3-Second 6-Axis Ring Buffer                  |  |
                      |  |  * WS2812 16x16 Matrix Controller (PIO + DMA)   |  |
                      |  |  * 38 kHz NEC IR Transmitter (PIO State Machine)|  |
                      |  |  * NEC Demodulating IR Receiver (PIO Interrupt) |  |
                      |  |  * DRV2605 Haptic Feedback (I2C0)               |  |
                      |  |  * Game State Machine, Health Bar & Shield Logic|  |
                      |  +-----------------------+-------------------------+  |
                      |                          |                             |
                      |                  Multicore Hardware                    |
                      |                    FIFO + Mutex                        |
                      |                          |                             |
                      |  +-----------------------v-------------------------+  |
                      |  | Core 1: TinyML Inference Engine                 |  |
                      |  |                                                 |  |
                      |  |  * Edge Impulse TFLite Micro Runtime            |  |
                      |  |  * 1650 ms Sliding Window Feature Extractor     |  |
                      |  |  * CMSIS-DSP Accelerated Neural Classifier      |  |
                      |  |  * K-Means Anomaly Detection Engine             |  |
                      |  +-------------------------------------------------+  |
                      +-------------------------------------------------------+
```

---

## Hardware Specifications

| Component | Part / IC | Interface | Description |
|---|---|---|---|
| **Microcontroller** | SparkFun Pro Micro RP2040 | Dual ARM Cortex-M0+ @ 133 MHz | Primary compute module with 264 KB SRAM and 2 MB Flash. |
| **Inertial Measurement** | Adafruit BNO085 | I2C0 (Fast Mode, 400 kHz) | 9-DOF IMU with onboard ARM Cortex-M0+ running Hillcrest SH-2 sensor fusion (Linear Accel + Rotation Vector). |
| **Haptics** | TI DRV2605L | I2C0 (Shared bus) | ERM/LRA haptic driver providing distinct wave sensations for spell casts, hits, and shield deployment. |
| **Display Matrix** | 16x16 WS2812B (256 LEDs) | PIO0 (Custom State Machine) | High-speed serialized 800 kHz pixel protocol driven directly by RP2040 PIO with no CPU bit-banging. |
| **IR Transmitter** | 940 nm High-Output IR LED | PIO1 (38 kHz Carrier Generator) | Hardware-timed NEC protocol encoding with 38 kHz carrier frequency modulation. |
| **IR Receiver** | TSOP38238 (38 kHz Demodulator) | PIO / Edge-triggered ISR | Demodulates incoming player commands and validates opponent player IDs. |
| **Shield Trigger** | Tactile Switch | GPIO 21 (Active High) | Hardware input to raise the defensive barrier and trigger hit immunity. |
| **Custom PCB** | 2-Layer KiCad Design | Multi-rail Power (5V / 3.3V) | Custom wand PCB integrating MCU, sensor breakouts, power regulation, and FET driver circuits. |

---

## Key Technical Challenges & Engineering Solutions

### 1. Dual-Core Real-Time Decoupling
Running TinyML neural network inference on continuous 6-axis sensor streams requires 60–90 ms of sustained computation per window. If executed synchronously on a single core:
- Sensor polling intervals would jitter, corrupting frequency-domain feature extraction.
- Incoming 38 kHz IR pulses would be dropped, causing missed hits during combat.
- LED matrix refresh routines would stall, resulting in visible visual stutter.

**Solution:** The system splits tasks across both RP2040 cores:
- **Core 0** handles strictly deterministic real-time tasks: reading the BNO085 at exactly 74 Hz, pushing samples to a circular buffer, rendering animations, managing IR transceiver state machines, and evaluating the duel state machine.
- **Core 1** operates as a co-processor dedicated solely to TinyML execution. When Core 0 detects an active gesture, it copies a 1650 ms sliding window (732 float features) into an exchange buffer and signals Core 1. Core 1 runs inference and returns a packed 32-bit result (`spell_id`, `confidence`, `anomaly_score`) via the RP2040 hardware FIFO.

### 2. False-Positive Filtering ("Leaky Bucket" Accumulator)
Natural wand handling and rapid arm movements can produce transient sensor spikes that momentarily fool an ML classifier.
- Instead of firing immediately on a single inference hit, the firmware uses an integrating leaky-bucket filter (`SPELL_TRIGGER_TARGET = 7`, `SPELL_DECAY_RATE = 2`).
- A spell is only cast when consecutive inferences consistently recognize the same gesture above the confidence threshold (`>= 0.90`).
- Individual spells use dynamic anomaly ceilings (e.g., `2.3` for Aguamenti vs. `2.6` for Stupefy) to cleanly reject out-of-distribution movements.

### 3. Hardware Peripheral Offloading via PIO
- **WS2812B Protocol:** Rather than tying up the CPU with tight assembly loops for the strict 800 kHz NZR timing, a custom PIO program streams pixel color buffers directly to the matrix.
- **38 kHz IR Modulation:** The 38 kHz carrier square wave and NEC pulse-distance bit-timing are generated in hardware using a PIO state machine, ensuring precise modulation independent of CPU load.

---

## Combat & Game Mechanics

- **Spells:**
  - **Aguamenti (Water Spell):** Forward-sweeping motion -> triggers blue matrix firework animation, distinct haptic vibration, and broadcasts NEC command `10` over IR. Deals 2 damage.
  - **Stupefy (Stun Spell):** Sharp diagonal slash -> triggers red matrix firework animation, heavy haptic jolt, and broadcasts NEC command `20` over IR. Deals 2 damage.
- **Protego (Shield):** Pressing the shield button deploys an animated protective blue barrier on the matrix for 1.5 seconds. During this window, any incoming IR hits are flushed from the queue and ignored (temporary immunity). A 10-second cooldown timer is enforced before the shield can be redeployed.
- **Health System:** A perimeter health bar tracks player health (initialized to 10 HP). When health drops to 0, an animated defeat screen is displayed and casting is locked out until respawn.
- **Multiplayer Duel Configuration:**
  - In [`firmware/src/sys_config.h`](firmware/src/sys_config.h), set `PLAYER_ID` to `1` or `2` to assign wand identity.
  - Set `ALLOW_SELF_HIT` to `1` for single-wand solo testing, or `0` for two-player competitive duels (to prevent bouncing off walls and self-inflicting damage).

---

## Hardware Pinout Map

| RP2040 GPIO | Function | Peripheral |
|---|---|---|
| **GPIO 4** | I2C0 SDA | BNO085 IMU & DRV2605 Haptic Driver |
| **GPIO 5** | I2C0 SCL | BNO085 IMU & DRV2605 Haptic Driver |
| **GPIO 6** | Input / PIO | TSOP38238 38 kHz IR Receiver |
| **GPIO 16** | Output / PIO | IR LED Driver (38 kHz modulated output) |
| **GPIO 20** | Output / PIO | WS2812B 16x16 LED Matrix Data Line |
| **GPIO 21** | Input (Pull-Down) | Shield Tactile Switch |

---

## Repository Structure

```
.
├── firmware/                   # Primary dual-core wand firmware (PlatformIO project)
│   ├── src/                    # Application source files
│   │   ├── main.c              # Core 0 loop, hardware init, duel logic, IR/LED handling
│   │   ├── sys_config.h        # Pin assignments, tuning thresholds, player identity
│   │   ├── ei_bridge.cpp       # Edge Impulse C++ wrapper for Core 1 inference
│   │   └── ei_bridge.h
│   ├── lib/                    # Device drivers and neural network model
│   │   ├── Adafruit_BNO08x/    # Hillcrest SH-2 IMU driver
│   │   ├── haptics/            # DRV2605 haptic motor driver
│   │   ├── ir_receiver/        # NEC IR decoder implementation
│   │   ├── ir_transmitter/     # NEC IR encoder & emitter
│   │   ├── led_matrix/         # Matrix animations (fireworks, shield, healthbar)
│   │   └── edge-impulse-model/ # Exported TinyML model with CMSIS-DSP kernels
│   ├── pio/                    # PIO assembly files (WS2812 timing, IR carrier)
│   └── platformio.ini          # Build configuration (Pico-SDK, multicore, CMSIS flags)
│
├── hardware/                   # KiCad electrical design & PCB layout
│   ├── ece362project.kicad_sch # Top-level schematic
│   ├── ece362project.kicad_pcb # 2-layer routed PCB layout
│   ├── *.kicad_sch             # Hierarchical schematic sheets (power, IMU, haptics, etc.)
│   ├── components/             # Custom footprints and symbol libraries
│   └── manufacturing/          # Fabrication gerber files, drill files, and BOM
│
├── prototypes/                 # Isolated subsystem test benches used during bring-up
│   ├── haptics/                # DRV2605 waveform testing
│   ├── imu/                    # BNO08x orientation & acceleration logging
│   ├── ir_receiver/            # IR receiver timing & decoding tests
│   ├── ir_transmitter/         # IR transmitter carrier & burst verification
│   └── led_matrix/             # LED animation test bench
│
└── archive/                    # Developmental prototype snapshots
```

---

## Building & Flashing

### Prerequisites
- [PlatformIO Core](https://platformio.org/) (CLI) or PlatformIO IDE extension for VS Code.
- Raspberry Pi Pico toolchain with Pico-SDK support.
- A SWD debugger (e.g. Raspberry Pi Picoprobe) or standard USB BOOTSEL drag-and-drop.

### Build
Navigate to the firmware directory and compile:
```bash
cd firmware
pio run
```

### Flash via Picoprobe
```bash
pio run -t upload
```

### Serial Monitor
To view real-time debug telemetry (gesture scores, anomaly ratings, hit events):
```bash
pio device monitor -b 115200
```
