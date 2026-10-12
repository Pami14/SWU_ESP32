# TMC2209 Stepper Sequence (Arduino Uno/Nano, standalone mode)

Plain AVR C++ (no Arduino framework) that drives a stepper motor through a BigTreeTech TMC2209 board in **standalone STEP/DIR mode** (no UART).

File: `src\main.cpp`

## Functionality

The program repeats this cycle forever and reverses the direction after each cycle:

| Phase | Duration | Speed | LED (D13) |
|---|---|---|---|
| Ramp up | 1.5 s | 10 rpm to 100 rpm (linear) | ON |
| Constant speed | 20 s | 100 rpm | ON |
| Brake (ramp down) | 1 s | 100 rpm to 10 rpm (linear), then stop | ON |
| Standstill | 5 s | 0 (motor holds position) | OFF |
| Reverse direction | | Next cycle runs the other way | |

Notes:
- The 20 s are counted at full speed, so the ramps are extra time.
- The motor keeps holding torque during the standstill, because the driver stays enabled.
- The onboard LED is on while the motor is spinning and off during the 5 s pause.
- The shaft position after a cycle is not exact, because the sequence is time-based, not step-counted.

## Configuration (top of the file)

| Constant | Default | Meaning |
|---|---|---|
| `MICROSTEPS` | 8 | Must match the MS1/MS2 wiring |
| `STEPS_PER_REV` | 200 x MICROSTEPS | 1600 for a 1.8 degree motor at 1/8 |
| `START_RPM` | 10 | Speed where the ramp starts and ends |
| `TARGET_RPM` | 100 | Full speed |
| `RAMP_UP_MS` | 1500 | Acceleration time |
| `RAMP_DOWN_MS` | 1000 | Braking time |
| `RUN_MS` | 20000 | Time at full speed |
| `STOP_MS` | 5000 | Standstill time before reversing |

If the motor stalls or skips steps while accelerating, increase `RAMP_UP_MS` (for example to 3000), lower `TARGET_RPM`, or raise the motor current slightly.

## How it works

- **Step generation:** Timer1 runs freely at 0.5 us per tick (prescaler 8 at 16 MHz). Every step is a 10 us pulse on STEP, and the rest of the step period is waited out using the timer. This keeps the speed accurate regardless of the calculation time.
- **Ramps:** `run_segment(rpm_start, rpm_end, duration_ms)` changes the speed linearly over time. With equal start and end speed it runs at constant speed.
- **Step period:** `period = 1 / (rpm x STEPS_PER_REV / 60)`.
  At 100 rpm and 1/8 microstepping that is 2667 steps/s, or 375 us per step.
- **Direction:** the DIR pin is set at the start of each cycle, with a 5 us setup time.

## Wiring

### Logic side (J1)

| BTT pin | Connect to |
|---|---|
| 1 EN | Arduino D7 (LOW = enabled), or GND for always enabled |
| 2 MS1 | GND |
| 3 MS2 | GND |
| 4 / 5 PDN_UART | GND (required in standalone mode) |
| 6 CLK | GND |
| 7 STEP | Arduino D2 |
| 8 DIR | Arduino D3 |

### Power and motor (J2)

| BTT pin | Connect to |
|---|---|
| 1 VM | Motor supply + (12 V or 24 V) |
| 2 GND | Motor supply - and Arduino GND |
| 3, 4 (A2, A1) | Motor coil A |
| 5, 6 (B1, B2) | Motor coil B |
| 7 VCC_IO | Arduino 5V |
| 8 GND | Arduino GND |

### Microstepping

| MS2 | MS1 | Microsteps |
|---|---|---|
| GND | GND | 8 |
| GND | 5V | 32 |
| 5V | GND | 64 |
| 5V | 5V | 16 |

All modes interpolate to 256 microsteps internally.

## Setting the motor current (Vref)

- Measure the voltage between the pot and GND while the board is powered.
- RMS current is roughly `I = Vref x 0.71` (Vref max 2.5 V).
- Set about 70-80% of the motor's rated current.
- Check the exact relationship for your board revision in the BigTreeTech documentation.

## Safety and power-up order

1. Connect everything with all power off. Add a 100 uF capacitor across VM/GND close to the driver (35 V or higher for 24 V).
2. Power the Arduino first (USB).
3. Switch on the motor supply.
4. To shut down, do it in reverse: motor supply off first, then USB.

Rules:
- Never connect or disconnect the motor while VM is powered.
- Do not hot-plug the DC connector into a live supply.
- Never connect the motor supply to VCC_IO or the Arduino 5V pin.
- A floating EN pin means the driver is enabled (internal pull-down). Tie EN firmly to GND or D7 to avoid noise.
- If the motor stalls, stop it quickly, because a stalled energized motor heats up.

## Building

- Target: ATmega328P at 16 MHz (Arduino Uno/Nano).
- Compile with avr-gcc or PlatformIO with optimization enabled (for example `-Os`), because `_delay_us()` needs it.
- `F_CPU` is defined as `16000000UL` in the file if the build system does not set it.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Motor buzzes but does not turn | Coil pairs mixed up, speed too high at start, or current too low |
| Motor does nothing | VM not powered, PDN_UART not on GND, grounds not common |
| Skips steps or stalls in ramp | Ramp too short, speed too high, Vref too low |
| Driver very hot | Vref too high |
| LED never turns on | Wrong board (LED not on D13) or program not running |

## Possible extensions

- Count steps for exact positioning.
- Switch to a Timer1 interrupt for step generation to free the CPU.
- Add UART (TMCStepper library) for software current control, StealthChop/SpreadCycle selection and diagnostics.
