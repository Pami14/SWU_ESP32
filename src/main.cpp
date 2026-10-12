#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

// ---- Pins (Arduino Uno/Nano) ----
// EN   = D7 = PD7 (active LOW)
// STEP = D2 = PD2
// DIR  = D3 = PD3
// LED  = D13 = PB5 (onboard status LED: ON while spinning, OFF during the pause)

// ---- Config ----
// MS1 = MS2 = GND -> 1/8 microstepping
static constexpr uint16_t MICROSTEPS    = 8;
static constexpr uint16_t STEPS_PER_REV = 200 * MICROSTEPS;   // 1.8 deg motor -> 1600

static constexpr float    START_RPM     = 10.0f;     // speed at which the ramp starts/ends
static constexpr float    TARGET_RPM    = 100.0f;
static constexpr uint32_t RAMP_UP_MS    = 1500;      // acceleration time
static constexpr uint32_t RAMP_DOWN_MS  = 1000;      // braking time
static constexpr uint32_t RUN_MS        = 20000;     // time at full speed
static constexpr uint32_t STOP_MS       = 5000;      // standstill before reversing

// ---- Timer1: free running, 0.5 us per tick (prescaler 8 @ 16 MHz) ----
static void timer_init() {
  TCCR1A = 0;
  TCCR1B = (1 << CS11);
}

// Busy-wait until 'ticks' have passed since 'start' (max 65535 ticks = 32 ms)
static inline void wait_since(uint16_t start, uint16_t ticks) {
  while ((uint16_t)(TCNT1 - start) < ticks) {}
}

// ---- Pin helpers ----
static void pins_init() {
  DDRD  |= (1 << DDD7) | (1 << DDD2) | (1 << DDD3);
  PORTD |= (1 << PORTD7);                              // driver disabled
  PORTD &= ~((1 << PORTD2) | (1 << PORTD3));
}

static void led_init() { DDRB |= (1 << DDB5); PORTB &= ~(1 << PORTB5); }
static void led_on()   { PORTB |=  (1 << PORTB5); }
static void led_off()  { PORTB &= ~(1 << PORTB5); }

static void driver_enable()  { PORTD &= ~(1 << PORTD7); }
static void driver_disable() { PORTD |=  (1 << PORTD7); }

static void set_dir(bool forward) {
  if (forward) PORTD |=  (1 << PORTD3);
  else         PORTD &= ~(1 << PORTD3);
  _delay_us(5);
}

static void wait_ms(uint32_t ms) {
  while (ms--) _delay_ms(1);
}

// Step period in timer ticks (0.5 us) for a given speed
static uint16_t period_ticks(float rpm) {
  float steps_per_s = rpm * STEPS_PER_REV / 60.0f;
  float period_us   = 1000000.0f / steps_per_s;
  float ticks       = period_us * 2.0f;
  if (ticks > 65000.0f) ticks = 65000.0f;
  return (uint16_t)ticks;
}

// Runs for duration_ms while the speed changes linearly from rpm_start to rpm_end.
// Use rpm_start == rpm_end for constant speed.
static void run_segment(float rpm_start, float rpm_end, uint32_t duration_ms) {
  const uint32_t duration_us = duration_ms * 1000UL;
  uint32_t elapsed_us = 0;

  while (elapsed_us < duration_us) {
    float frac = (float)elapsed_us / (float)duration_us;
    float rpm  = rpm_start + (rpm_end - rpm_start) * frac;
    uint16_t period = period_ticks(rpm);

    uint16_t t0 = TCNT1;
    PORTD |= (1 << PORTD2);          // STEP high
    wait_since(t0, 20);              // 10 us pulse
    PORTD &= ~(1 << PORTD2);         // STEP low
    wait_since(t0, period);          // complete the step period

    elapsed_us += period / 2;
  }
}

static void run_cycle(bool forward) {
  set_dir(forward);
  led_on();                                            // spinning -> LED on
  run_segment(START_RPM,  TARGET_RPM, RAMP_UP_MS);     // ramp up
  run_segment(TARGET_RPM, TARGET_RPM, RUN_MS);         // 20 s at 100 rpm
  run_segment(TARGET_RPM, START_RPM,  RAMP_DOWN_MS);   // brake
  led_off();                                           // standstill -> LED off
  wait_ms(STOP_MS);                                    // 5 s standstill (motor holds position)
}

int main() {
  pins_init();
  led_init();
  timer_init();
  wait_ms(500);          // let the TMC2209 power up
  driver_enable();

  bool forward = true;
  while (true) {
    run_cycle(forward);
    forward = !forward;  // reverse for the next cycle
  }
}