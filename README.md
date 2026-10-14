# TMC2209 Stepper Sequence (ESP32-WROOM-32, ESP-IDF, standalone mode)

C++ project for ESP-IDF that drives a stepper motor through a BigTreeTech TMC2209 board in **standalone STEP/DIR mode** (no UART). It reports over serial whether the motor is spinning or stopped.

File: `main/main.cpp`

## Functionality

The program repeats this cycle forever and reverses the direction after each cycle:

| Phase | Duration | Speed | LED (GPIO4) | Serial state |
|---|---|---|---|---|
| Ramp up | 1.5 s | 10 rpm to 100 rpm (linear) | ON | SPINNING |
| Constant speed | 20 s | 100 rpm | ON | SPINNING |
| Brake (ramp down) | 1 s | 100 rpm to 10 rpm (linear), then stop | ON | SPINNING |
| Standstill | 5 s | 0 (motor holds position) | OFF | STOPPED |
| Reverse direction | | Next cycle runs the other way | | |

Notes:
- The 20 s are counted at full speed, so the ramps are extra time.
- The motor keeps holding torque during the standstill, because the driver stays enabled.
- The LED is on while the motor is spinning and off during the pause.
- The sequence is time-based, so the shaft position after a cycle is not exact.

## Serial output

Open it with `idf.py monitor` (115200 baud, USB port). The messages are sent through `printf` on UART0.

On a change of state:
```
TMC2209 sequence started, driver enabled
STATE: STOPPED
STATE: SPINNING (forward)
STATE: STOPPED
```

Once per second while spinning:
```
SPINNING  dir=forward  speed=100 rpm
```

Once per second during the pause:
```
STOPPED   5 s until next cycle
```

## Configuration (top of `main.cpp`)

| Constant | Default | Meaning |
|---|---|---|
| `PIN_STEP` | GPIO25 | STEP output |
| `PIN_DIR` | GPIO26 | DIR output |
| `PIN_EN` | GPIO27 | Enable output, active LOW |
| `PIN_LED` | GPIO4 | Status LED (external LED + 330 ohm resistor) |
| `MICROSTEPS` | 8 | Must match the MS1/MS2 wiring |
| `STEPS_PER_REV` | 200 x MICROSTEPS | 1600 for a 1.8 degree motor at 1/8 |
| `START_RPM` | 10 | Speed where the ramp starts and ends |
| `TARGET_RPM` | 100 | Full speed |
| `RAMP_UP_MS` | 1500 | Acceleration time |
| `RAMP_DOWN_MS` | 1000 | Braking time |
| `RUN_MS` | 20000 | Time at full speed |
| `STOP_MS` | 5000 | Standstill time before reversing |
| `UPDATE_MS` | 10 | Interval for speed updates |

If the motor stalls or skips steps while accelerating, increase `RAMP_UP_MS` (for example to 3000), lower `TARGET_RPM`, or raise the motor current slightly.

## How it works

- **Step generation:** the LEDC peripheral generates the step signal in hardware as a 50 % duty square wave (10 bit resolution, low-speed mode, timer 0). The TMC2209 steps on each rising edge.
- **Ramps:** `run_segment(rpm_start, rpm_end, duration_ms)` changes the LEDC frequency every 10 ms, linearly over time. With equal start and end speed it runs at constant speed.
- **Step frequency:** `f = rpm x STEPS_PER_REV / 60`. At 100 rpm and 1/8 microstepping that is 2667 Hz, and at 10 rpm it is 267 Hz.
- **Standstill:** the duty is set to 0, which holds STEP low.
- **Direction:** the DIR pin is set at the start of each cycle, followed by a 2 ms setup time.
- **Limitation:** changing the LEDC frequency while it runs can cause a very small timing glitch at each update. This is not noticeable on the motor. For exact step counting, use the RMT peripheral or a hardware timer instead.

## Wiring

The ESP32 uses **3.3 V logic and its pins are not 5 V tolerant**. VCC_IO must go to 3V3.

### Logic side (J1)

| BTT pin | Connect to |
|---|---|
| 1 EN | GPIO27 (LOW = enabled) |
| 2 MS1 | GND |
| 3 MS2 | GND |
| 4 / 5 PDN_UART | GND (required in standalone mode) |
| 6 CLK | GND |
| 7 STEP | GPIO25 |
| 8 DIR | GPIO26 |

### Power and motor (J2)

| BTT pin | Connect to |
|---|---|
| 1 VM | Motor supply + (12 V or 24 V) |
| 2 GND | Motor supply - and ESP32 GND |
| 3, 4 (A2, A1) | Motor coil A |
| 5, 6 (B1, B2) | Motor coil B |
| 7 VCC_IO | ESP32 3V3 |
| 8 GND | ESP32 GND |

### LED

GPIO4, through a 330 ohm resistor, to an LED, to GND. If your board has a user LED on another GPIO, change `PIN_LED`.

### Microstepping

| MS2 | MS1 | Microsteps |
|---|---|---|
| GND | GND | 8 |
| GND | 3V3 | 32 |
| 3V3 | GND | 64 |
| 3V3 | 3V3 | 16 |

All modes interpolate to 256 microsteps internally. Connect MS1/MS2 to GND or 3V3 (VCC_IO), never to the motor supply.

## Setting the motor current (Vref)

- Measure the voltage between the pot and GND while the board is powered.
- RMS current is roughly `I = Vref x 0.71` (Vref max 2.5 V).
- Set about 70-80% of the motor's rated current.
- Check the exact relationship for your board revision in the BigTreeTech documentation.

## Safety and power-up order

1. Connect everything with all power off. Add a 100 uF capacitor across VM/GND close to the driver (35 V or higher for 24 V).
2. Power the ESP32 first (USB).
3. Switch on the motor supply.
4. To shut down, do it in reverse: motor supply off first, then USB.

Rules:
- Never connect or disconnect the motor while VM is powered.
- Do not hot-plug the DC connector into a live supply.
- Never connect the motor supply to VCC_IO or the ESP32 pins.
- A floating EN pin means the driver is enabled (internal pull-down). Keep EN connected to GPIO27 or tie it to GND.
- If the motor stalls, stop it quickly, because a stalled energized motor heats up.

## Building and flashing

```
idf.py build
idf.py flash monitor
```

- Target: ESP32 (ESP32-WROOM-32, for example the AZ-Delivery DevKitC V4).
- `main/CMakeLists.txt` registers `main.cpp` as the source of the `main` component.
- No special `sdkconfig` settings are needed.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Motor buzzes but does not turn | Coil pairs mixed up, speed too high at start, or current too low |
| Motor does nothing | VM not powered, PDN_UART not on GND, grounds not common |
| Skips steps or stalls in ramp | Ramp too short, speed too high, Vref too low |
| Driver very hot | Vref too high |
| LED never turns on | LED wired the wrong way round, wrong GPIO, or program not running |
| No serial output | Wrong COM port, or monitor baud rate not 115200 |

## Possible extensions

- Count steps for exact positioning (RMT peripheral or hardware timer).
- Add UART (TMCStepper library) for software current control, StealthChop/SpreadCycle selection and diagnostics.
- Send the status as JSON or with timestamps for a PC program.