// TMC2209 standalone STEP/DIR sequence on ESP32 (ESP-IDF)
// Cycle: ramp up -> 20 s at 100 rpm -> brake -> 5 s standstill (LED off) -> reverse
//
// Step pulses are generated in hardware by the LEDC peripheral (50 % duty square wave).
// The step frequency is changed every 10 ms to create the ramps.
// MS1 = MS2 = GND -> 1/8 microstepping.

#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"

// ---- Pins (all safe GPIOs on ESP32-WROOM-32 DevKitC) ----
static constexpr gpio_num_t PIN_STEP = GPIO_NUM_25;
static constexpr gpio_num_t PIN_DIR  = GPIO_NUM_26;
static constexpr gpio_num_t PIN_EN   = GPIO_NUM_27;   // active LOW
static constexpr gpio_num_t PIN_LED  = GPIO_NUM_4;    // external LED + 330 ohm resistor to GND

// ---- Config ----
static constexpr uint32_t MICROSTEPS    = 8;
static constexpr uint32_t STEPS_PER_REV = 200 * MICROSTEPS;   // 1.8 deg motor -> 1600

static constexpr float    START_RPM     = 10.0f;
static constexpr float    TARGET_RPM    = 100.0f;
static constexpr uint32_t RAMP_UP_MS    = 1500;
static constexpr uint32_t RAMP_DOWN_MS  = 1000;
static constexpr uint32_t RUN_MS        = 20000;
static constexpr uint32_t STOP_MS       = 5000;

static constexpr uint32_t UPDATE_MS     = 10;             // speed update interval

// ---- LEDC (step generator) ----
static constexpr ledc_mode_t     STEP_MODE    = LEDC_LOW_SPEED_MODE;
static constexpr ledc_timer_t    STEP_TIMER   = LEDC_TIMER_0;
static constexpr ledc_channel_t  STEP_CHANNEL = LEDC_CHANNEL_0;
static constexpr uint32_t        STEP_DUTY    = 512;      // 50 % of 10 bit

static void step_gen_init() {
  ledc_timer_config_t t = {};
  t.speed_mode      = STEP_MODE;
  t.duty_resolution = LEDC_TIMER_10_BIT;
  t.timer_num       = STEP_TIMER;
  t.freq_hz         = 1000;
  t.clk_cfg         = LEDC_AUTO_CLK;
  ledc_timer_config(&t);

  ledc_channel_config_t c = {};
  c.gpio_num   = PIN_STEP;
  c.speed_mode = STEP_MODE;
  c.channel    = STEP_CHANNEL;
  c.intr_type  = LEDC_INTR_DISABLE;
  c.timer_sel  = STEP_TIMER;
  c.duty       = 0;                 // no pulses yet
  c.hpoint     = 0;
  ledc_channel_config(&c);
}

static void step_gen_set_rpm(float rpm) {
  uint32_t freq = (uint32_t)(rpm * STEPS_PER_REV / 60.0f);
  if (freq < 100) freq = 100;
  ledc_set_freq(STEP_MODE, STEP_TIMER, freq);
}

static void step_gen_start() {
  ledc_set_duty(STEP_MODE, STEP_CHANNEL, STEP_DUTY);
  ledc_update_duty(STEP_MODE, STEP_CHANNEL);
}

static void step_gen_stop() {
  ledc_set_duty(STEP_MODE, STEP_CHANNEL, 0);    // STEP stays LOW
  ledc_update_duty(STEP_MODE, STEP_CHANNEL);
}

// ---- GPIO helpers ----
static void gpio_setup() {
  gpio_config_t io = {};
  io.pin_bit_mask = (1ULL << PIN_DIR) | (1ULL << PIN_EN) | (1ULL << PIN_LED);
  io.mode         = GPIO_MODE_OUTPUT;
  io.pull_up_en   = GPIO_PULLUP_DISABLE;
  io.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io.intr_type    = GPIO_INTR_DISABLE;
  gpio_config(&io);

  gpio_set_level(PIN_EN, 1);    // driver disabled
  gpio_set_level(PIN_DIR, 0);
  gpio_set_level(PIN_LED, 0);
}

static void driver_enable()  { gpio_set_level(PIN_EN, 0); }
static void led(bool on)     { gpio_set_level(PIN_LED, on ? 1 : 0); }

static bool g_forward = true;                   // current direction, used for the serial status

static const char *dir_text() { return g_forward ? "forward" : "reverse"; }

static void set_dir(bool forward) {
  g_forward = forward;
  gpio_set_level(PIN_DIR, forward ? 1 : 0);
  vTaskDelay(pdMS_TO_TICKS(2));                 // DIR setup time
}

// Linear speed change from rpm_start to rpm_end over duration_ms
// (equal values = constant speed). Prints the status once per second.
static void run_segment(float rpm_start, float rpm_end, uint32_t duration_ms) {
  const int64_t t0 = esp_timer_get_time();
  uint32_t next_report_ms = 0;

  while (true) {
    uint32_t elapsed_ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
    if (elapsed_ms >= duration_ms) break;

    float frac = (float)elapsed_ms / (float)duration_ms;
    float rpm  = rpm_start + (rpm_end - rpm_start) * frac;
    step_gen_set_rpm(rpm);

    if (elapsed_ms >= next_report_ms) {
      printf("SPINNING  dir=%s  speed=%d rpm\n", dir_text(), (int)(rpm + 0.5f));
      next_report_ms += 1000;
    }

    vTaskDelay(pdMS_TO_TICKS(UPDATE_MS));
  }
}

static void run_cycle(bool forward) {
  set_dir(forward);

  led(true);                                       // spinning -> LED on
  printf("STATE: SPINNING (%s)\n", dir_text());
  step_gen_set_rpm(START_RPM);
  step_gen_start();

  run_segment(START_RPM,  TARGET_RPM, RAMP_UP_MS);     // ramp up
  run_segment(TARGET_RPM, TARGET_RPM, RUN_MS);         // 20 s at 100 rpm
  run_segment(TARGET_RPM, START_RPM,  RAMP_DOWN_MS);   // brake

  step_gen_stop();
  led(false);                                      // standstill -> LED off
  printf("STATE: STOPPED\n");

  // 5 s pause (motor holds position), status once per second
  for (uint32_t waited = 0; waited < STOP_MS; waited += 1000) {
    printf("STOPPED   %u s until next cycle\n", (unsigned)((STOP_MS - waited) / 1000));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

extern "C" void app_main(void) {
  gpio_setup();
  step_gen_init();

  vTaskDelay(pdMS_TO_TICKS(500));   // let the TMC2209 power up
  driver_enable();
  printf("TMC2209 sequence started, driver enabled\n");
  printf("STATE: STOPPED\n");

  bool forward = true;
  while (true) {
    run_cycle(forward);
    forward = !forward;
  }
}