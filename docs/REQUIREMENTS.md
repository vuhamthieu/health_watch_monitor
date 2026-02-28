# Health Watch Monitor — Project Requirements & Architecture

> **Status:** Draft v1.0 — 2026-02-28
> Fill in edge cases and confirm pin assignments before writing code.

---

## 1. Project Overview

A wrist-worn health monitoring device built around the STM32F103C8T6 microcontroller running FreeRTOS. The device continuously measures heart rate, blood oxygen saturation (SpO2), and step count, displaying all data on a 1.3″ OLED screen. A Bluetooth module enables data streaming to a companion phone app.

---

## 2. Hardware Components

| Component | Function | Interface |
|-----------|----------|-----------|
| STM32F103C8T6 (Mini) | Main MCU @ 64 MHz | — |
| OLED 1.3″ SH1106 (128×64) | Primary display | I2C1 |
| MPU-6050 | Accelerometer + Gyroscope | I2C1 |
| MAX30102 | Heart rate + SpO2 sensor | I2C1 |
| JDY-31 | Bluetooth 2.0 SPP module | USART1 |
| BTN_UP | Navigate up / previous | PA3 (GPIO, pull-up) |
| BTN_DOWN | Navigate down / next | PA2 (GPIO, pull-up) |
| BTN_SELECT | Confirm / enter submenu | PA15 (GPIO, pull-up) |
| BTN_BACK | Go back / cancel | PA0 (EXTI0, pull-up) |
| LED | Built-in status LED | PC13 |

---

## 3. Hardware Pin Mapping

```
STM32F103C8T6 Mini
─────────────────────────────────────────
 PA0  (WKUP / EXTI0)   → BTN_BACK        [EXTI, falling-edge, pull-up]
 PA2                   → BTN_DOWN        [GPIO Input, pull-up, polled]
 PA3                   → BTN_UP          [GPIO Input, pull-up, polled]
 PA9  (USART1_TX)      → JDY-31 RX
 PA10 (USART1_RX)      → JDY-31 TX
 PA13 (SWD)            → SWDIO
 PA14 (SWD)            → SWCLK
 PA15                  → BTN_SELECT      [GPIO Input, pull-up, polled]
 PB6  (I2C1_SCL)       → OLED + MPU6050 + MAX30102
 PB7  (I2C1_SDA)       → OLED + MPU6050 + MAX30102
 PC13                  → Built-in LED    [Active LOW]
─────────────────────────────────────────

I2C Bus Addresses:
  OLED  SH1106  : 0x3C  (SA0 = GND)  or 0x3D (SA0 = VCC)
  MPU-6050       : 0x68  (AD0 = GND)  or 0x69 (AD0 = VCC)
  MAX30102       : 0x57  (fixed)

USART1:
  Baud Rate : 9600 (JDY-31 default) or 38400
  Mode      : Async, 8N1

Clock:
  HSI 8 MHz → PLL ×16 → SYSCLK 64 MHz
  APB1 = 32 MHz | APB2 = 64 MHz
```

---

## 4. FreeRTOS Task Architecture

| Task | Priority | Stack | Responsibility |
|------|----------|-------|----------------|
| `defaultTask` | Normal (0) | 256 W | System heartbeat, IWDG feed |
| `buttonTask` | Normal (0) | 256 W | Poll buttons, detect press/long-press, publish events |
| `sensorTask` | AboveNormal (+1) | 512 W | Read MPU-6050, MAX30102; run algorithms; update shared data |
| `uiTask` | Normal (0) | 512 W | Render OLED, manage menu state machine |
| `bleTask` | BelowNormal (−1) | 512 W | JDY-31 TX/RX, data formatting, command parsing |
| `powerTask` | High (+2) | 256 W | Monitor inactivity, manage power states, handle sleep |

### IPC Objects

| Object | Type | Producers → Consumers |
|--------|------|----------------------|
| `xButtonEventQueue` | Queue (depth 8) | `buttonTask` → `uiTask`, `powerTask` |
| `xSensorDataMutex` | Mutex | `sensorTask` ↔ `uiTask`, `bleTask` |
| `xI2cMutex` | Mutex (HAL) | all I2C users (sh1106, mpu6050, max30102) |
| `xPowerEventQueue` | Queue (depth 4) | `buttonTask`, `sensorTask` → `powerTask` |
| `xBleQueue` | Queue (depth 16) | `sensorTask` → `bleTask` |
| `xWakeEventGroup` | EventGroup | wrist-gesture (MPU), button → `powerTask` |

---

## 5. Functional Requirements

### F1 — Display & UI (OLED SH1106 128×64)

| ID | Requirement |
|----|-------------|
| F1.1 | Display real-time heart rate in BPM on main screen |
| F1.2 | Display SpO2 percentage on main screen |
| F1.3 | Display step count on main screen |
| F1.4 | Display software clock (HH:MM) derived from FreeRTOS tick |
| F1.5 | Display Bluetooth connection status icon |
| F1.6 | OLED refresh rate: max 10 Hz (100 ms period in `uiTask`) |
| F1.7 | Show "---" for any metric with invalid/no-sensor reading |
| F1.8 | Display auto-dims after configurable idle timeout |
| F1.9 | Sleep screen: blank or minimal animation (power saving) |

### F2 — Heart Rate Monitoring (MAX30102)

| ID | Requirement |
|----|-------------|
| F2.1 | Continuous heart rate measurement; update every 1 s |
| F2.2 | Valid range: 40–200 BPM; display "---" outside this range |
| F2.3 | Alert indicator if HR > 120 BPM (tachycardia) or < 50 BPM (bradycardia) |
| F2.4 | Use moving-average or Pan-Tompkins peak detection |
| F2.5 | Detect absence of finger (low IR signal) → show "NO FINGER" |
| F2.6 | Debounce: discard single erratic readings (±30 BPM from previous) |

### F3 — SpO2 Monitoring (MAX30102)

| ID | Requirement |
|----|-------------|
| F3.1 | Continuous SpO2 measurement; update every 2 s |
| F3.2 | Valid range: 80–100%; display "---" outside range |
| F3.3 | Alert indicator if SpO2 < 95% |
| F3.4 | Share same MAX30102 sampling with heart rate (both from red + IR LEDs) |
| F3.5 | Display "NO FINGER" if sensor not in contact |

### F4 — Step Counter (MPU-6050 Accelerometer)

| ID | Requirement |
|----|-------------|
| F4.1 | Detect steps using magnitude threshold on acc vector |
| F4.2 | Sampling rate: 100 Hz from MPU-6050 |
| F4.3 | Step count accumulates during the day; reset at midnight (or manual) |
| F4.4 | Daily step goal indicator (default: 10,000 steps) |
| F4.5 | Estimate distance: steps × avg_stride_length (default 0.75 m) |
| F4.6 | Estimate calories: steps × 0.04 kcal (configurable) |

### F5 — Activity Detection (MPU-6050)

| ID | Requirement |
|----|-------------|
| F5.1 | Classify: STATIONARY, WALKING, RUNNING based on acc magnitude |
| F5.2 | Detect wrist-raise gesture → wake display from sleep |
| F5.3 | Motion threshold for wake: configurable in `app_config.h` |

### F6 — Bluetooth (JDY-31 SPP)

| ID | Requirement |
|----|-------------|
| F6.1 | Device name: "HealthWatch" (set via AT command at startup if not paired) |
| F6.2 | Baud rate: 9600 bps (default JDY-31) |
| F6.3 | Periodic data packet every 5 s (when connected): `HR,SPO2,STEPS,TIME\r\n` |
| F6.4 | Accept commands: `SYNC_TIME:<epoch>`, `RESET_STEPS`, `GET_DATA` |
| F6.5 | BT connection status: CONNECTED / DISCONNECTED icon on OLED |
| F6.6 | Buffer last 10 data points when disconnected; flush on reconnect |
| F6.7 | Handle UART RX overrun and framing errors gracefully |

### F7 — Power Management

| ID | Requirement |
|----|-------------|
| F7.1 | **Active mode**: all sensors running, full display brightness |
| F7.2 | **Dim mode**: after 30 s inactivity → reduce display contrast to 30% |
| F7.3 | **Sleep mode**: after 60 s inactivity → OLED off, sensor polling rate reduced to 0.5 Hz |
| F7.4 | **Wake triggers**: any button press, wrist-raise gesture, BT incoming data |
| F7.5 | Long press BACK (>2 s) → show Sleep Options menu on OLED |
| F7.6 | Sleep Options: "Sleep Now" / "Cancel" |
| F7.7 | Dim/sleep timeouts configurable via Settings menu |

### F8 — Button Controls

| ID | Requirement |
|----|-------------|
| F8.1 | **UP**: scroll up / increment value |
| F8.2 | **DOWN**: scroll down / decrement value |
| F8.3 | **SELECT**: confirm / enter submenu / start measurement |
| F8.4 | **BACK** (short press, <2 s): go back to parent screen |
| F8.5 | **BACK** (long press, ≥2 s): open Sleep Options overlay |
| F8.6 | Any button press wakes device from sleep/dim state |
| F8.7 | Button debounce time: 20 ms |
| F8.8 | Auto-repeat for UP/DOWN when held: after 500 ms hold, repeat every 150 ms |

---

## 6. Non-Functional Requirements

| ID | Category | Requirement |
|----|----------|-------------|
| NF1 | Performance | `sensorTask` runs at AboveNormal priority; sensor read cycle ≤ 50 ms |
| NF2 | Performance | OLED render cycle ≤ 100 ms; no blocking I2C calls in `uiTask` (use cached data) |
| NF3 | Memory | Total FreeRTOS heap: 16 KB — do NOT exceed 80% usage |
| NF4 | Memory | Flash ≤ 64 KB (STM32F103C8); RAM ≤ 20 KB |
| NF5 | Reliability | IWDG watchdog fed by `defaultTask`; timeout = 2 s |
| NF6 | Reliability | I2C bus recovery (reset SCL line) on HAL_TIMEOUT or HAL_ERROR |
| NF7 | Reliability | FreeRTOS stack overflow hook enabled (`configCHECK_FOR_STACK_OVERFLOW = 2`) |
| NF8 | Safety | All shared sensor data protected by `xSensorDataMutex` |
| NF9 | Code Quality | No `HAL_Delay()` inside any FreeRTOS task — use `osDelay()` only |
| NF10 | Code Quality | All modules compile cleanly with `-Wall -Wextra` |

---

## 7. Screen & Menu Flow

```
┌─────────────────────────────────────┐
│           MAIN SCREEN               │
│  ♡ 75 bpm    SpO2 98%   08:32      │
│  Steps: 4,231   BT: ●               │
│  [UP/DOWN to cycle screens]         │
└─────────────────────────────────────┘
         ↑ UP / DOWN ↓
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│  HEART   │  │  SpO2    │  │  STEPS   │  │   BT     │  │ SETTINGS │
│  RATE    │  │ DETAIL   │  │ DETAIL   │  │ STATUS   │  │          │
│  DETAIL  │  │          │  │          │  │          │  │  >  Bright│
│  75 bpm  │  │  98 %    │  │  4,231   │  │ CONN'ED  │  │  > BT    │
│          │  │  ●●●●○   │  │  3.2 km  │  │ "Phone1" │  │  > Steps │
│  [SELECT │  │          │  │  128 cal │  │          │  │  > Sleep │
│  to hold]│  │          │  │  [SELECT │  │          │  │  > About │
└──────────┘  └──────────┘  │  = reset]│  └──────────┘  └──────────┘
                             └──────────┘

BACK long press (from any screen):
┌─────────────────────────────────────┐
│  ┌─────────────────────────────┐    │
│  │      SLEEP OPTIONS          │    │
│  │   >  Sleep Now              │    │
│  │      Cancel                 │    │
│  └─────────────────────────────┘    │
└─────────────────────────────────────┘
```

---

## 8. Sensor Data Packet Format (Bluetooth)

```
Periodic (every 5s):
  HW:<hr>,<spo2>,<steps>,<hh:mm>\r\n
  Example: HW:75,98,4231,08:32\r\n

Commands received:
  SYNC_TIME:<seconds_since_midnight>\r\n
  RESET_STEPS\r\n
  GET_DATA\r\n

Responses:
  OK\r\n
  ERR:<reason>\r\n
```

---

## 9. Edge Cases (To Refine)

> These are known edge cases to address in implementation — add more as you think of them.

| # | Edge Case | Handling Strategy |
|---|-----------|-------------------|
| EC1 | Finger removed mid-measurement (MAX30102) | Detect low IR (<10,000 raw); set HR/SpO2 = INVALID; display "---" |
| EC2 | I2C bus hang (sensor not responding) | HAL timeout + GPIO bit-bang bus reset; retry × 3 then mark sensor FAULT |
| EC3 | FreeRTOS heap exhaustion | Enable `configUSE_MALLOC_FAILED_HOOK`; log fault, halt safely |
| EC4 | Stack overflow in any task | `vApplicationStackOverflowHook` → toggle LED, halt |
| EC5 | BT disconnects during data send | Detect UART TX error; buffer data; retry on reconnect |
| EC6 | BACK long press during active measurement | Pause measurement display, show sleep menu overlay |
| EC7 | UP/DOWN held continuously | Auto-repeat after 500 ms, 150 ms interval |
| EC8 | Multiple buttons pressed simultaneously | Only the first button registered; lock for 50 ms |
| EC9 | Step counter overflow (uint32 > 4B steps) | Reset to 0; extremely unlikely in practice |
| EC10 | Power loss during step count update | Steps are in RAM only (volatile); accepted data loss on power loss |
| EC11 | JDY-31 AT mode detection at boot | Check STATE pin if wired; else trust fixed baud configuration |
| EC12 | OLED I2C address conflict with sensors | Confirmed no conflict: SH1106=0x3C, MPU6050=0x68, MAX30102=0x57 |
| EC13 | MPU-6050 gives erratic readings on wrist raise | Apply low-pass filter; require 3 consecutive detections before waking |
| EC14 | Clock drift (no RTC) | Note: no external 32kHz crystal; use FreeRTOS tick as time base. Sync via BT `SYNC_TIME` command |

---

## 10. Project File Structure

```
health_watch_monitor/
├── Makefile
├── .vscode/
│   ├── c_cpp_properties.json
│   ├── tasks.json
│   ├── launch.json
│   └── settings.json
├── docs/
│   └── REQUIREMENTS.md            ← this file
├── Core/
│   ├── Inc/
│   │   ├── main.h                 [CubeIDE generated]
│   │   ├── FreeRTOSConfig.h       [CubeIDE generated]
│   │   ├── app_config.h           ← GPIO pins, I2C addr, timing constants
│   │   ├── sensor_data.h          ← shared data structs + FreeRTOS IPC handles
│   │   ├── button.h               ← button event enum + API
│   │   ├── sh1106.h               ← SH1106 low-level I2C driver
│   │   ├── oled.h                 ← high-level display API (text, icons, pages)
│   │   ├── mpu6050.h              ← MPU-6050 driver
│   │   ├── max30102.h             ← MAX30102 driver
│   │   ├── jdy31.h                ← JDY-31 Bluetooth driver
│   │   ├── ui_menu.h              ← UI/menu state machine
│   │   ├── power_manager.h        ← power state machine
│   │   ├── step_counter.h         ← pedometer algorithm
│   │   ├── heart_rate.h           ← HR signal processing
│   │   └── spo2.h                 ← SpO2 calculation
│   └── Src/
│       ├── main.c                 [CubeIDE generated — init only]
│       ├── freertos.c             [CubeIDE generated — task stubs]
│       ├── sensor_data.c          ← IPC object creation
│       ├── button.c               ← button polling, debounce, long-press
│       ├── sh1106.c               ← SH1106 driver implementation
│       ├── oled.c                 ← OLED page rendering
│       ├── mpu6050.c              ← MPU-6050 implementation
│       ├── max30102.c             ← MAX30102 implementation
│       ├── jdy31.c                ← JDY-31 implementation
│       ├── ui_menu.c              ← menu state machine
│       ├── power_manager.c        ← power management
│       ├── step_counter.c         ← pedometer
│       ├── heart_rate.c           ← HR algorithm
│       └── spo2.c                 ← SpO2 algorithm
├── Drivers/                       [CubeIDE generated — do not modify]
├── Middlewares/                   [CubeIDE generated — do not modify]
└── STM32F103C8TX_FLASH.ld         [CubeIDE generated]
```

---

## 11. Development Phases

### Phase 1 — Build System & Skeleton ✅
- [x] Makefile (arm-none-eabi-gcc, OpenOCD flash)
- [x] VS Code IntelliSense, tasks, debug launch
- [x] All header/source stubs created

### Phase 2 — Hardware Bring-up
- [ ] Verify I2C communication to each sensor (scan bus)
- [ ] SH1106 OLED: draw text, test all fonts
- [ ] MPU-6050: read raw accel/gyro data
- [ ] MAX30102: read IR/Red LED raw values
- [ ] JDY-31: AT command test via USART1

### Phase 3 — Driver Implementation
- [ ] SH1106 full driver (framebuffer, text, bitmaps)
- [ ] MPU-6050 full driver (DMP or raw, calibration)
- [ ] MAX30102 full driver (FIFO read, LED control)
- [ ] JDY-31 command/response parser

### Phase 4 — Algorithm Implementation
- [ ] Step counter (accelerometer peak detection)
- [ ] Heart rate (IR LED peak detection, BPM calculation)
- [ ] SpO2 (Red/IR ratio → %SpO2 lookup)

### Phase 5 — UI & Menu
- [ ] Menu state machine (all screens from §7)
- [ ] OLED page rendering (fonts, icons)
- [ ] Button handling (debounce, long press, auto-repeat)

### Phase 6 — Integration & FreeRTOS
- [ ] Wire all tasks to real implementations
- [ ] FreeRTOS queue/mutex integration
- [ ] Power manager state machine

### Phase 7 — Polish & Edge Cases
- [ ] All edge cases from §9
- [ ] IWDG watchdog integration
- [ ] Memory usage audit
- [ ] BT data streaming + command parsing
