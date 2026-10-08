#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

// ---- Config ----
static constexpr uint32_t BAUD       = 9600;
static constexpr uint16_t UBRR_VALUE = (F_CPU / (16UL * BAUD)) - 1;  // 103 @ 16 MHz
static constexpr uint8_t  DRIVER_ADDR = 0;                           // MS1 = MS2 = GND

// ---- TMC2209 registers ----
static constexpr uint8_t REG_GCONF = 0x00;
static constexpr uint8_t REG_IFCNT = 0x02;
static constexpr uint8_t REG_IOIN  = 0x06;

static constexpr uint32_t GCONF_PDN_DISABLE = (1UL << 6);  // use PDN_UART as UART

// ---- UART ----
static void uart_init() {
  UBRR0H = UBRR_VALUE >> 8;
  UBRR0L = UBRR_VALUE & 0xFF;
  UCSR0B = (1 << RXEN0) | (1 << TXEN0);
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);   // 8N1
}

static void uart_tx(uint8_t b) {
  while (!(UCSR0A & (1 << UDRE0))) {}
  UDR0 = b;
}

// Returns false on timeout (~10 ms)
static bool uart_rx(uint8_t &b) {
  for (uint8_t i = 0; i < 100; i++) {
    if (UCSR0A & (1 << RXC0)) {
      b = UDR0;
      return true;
    }
    _delay_us(100);
  }
  return false;
}

static void uart_flush_rx() {
  while (UCSR0A & (1 << RXC0)) { (void)UDR0; }
}

// ---- TMC2209 protocol ----
static uint8_t crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t cur = data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if ((crc >> 7) ^ (cur & 0x01)) crc = (crc << 1) ^ 0x07;
      else                           crc = (crc << 1);
      cur >>= 1;
    }
  }
  return crc;
}

static bool tmc_write(uint8_t reg, uint32_t value) {
  uint8_t d[8] = {
    0x05, DRIVER_ADDR, (uint8_t)(reg | 0x80),
    (uint8_t)(value >> 24), (uint8_t)(value >> 16),
    (uint8_t)(value >> 8),  (uint8_t)value, 0
  };
  d[7] = crc8(d, 7);

  uart_flush_rx();
  for (uint8_t i = 0; i < 8; i++) uart_tx(d[i]);

  // discard the 8 echoed bytes
  uint8_t tmp;
  for (uint8_t i = 0; i < 8; i++) {
    if (!uart_rx(tmp)) return false;
  }
  return true;
}

static bool tmc_read(uint8_t reg, uint32_t &value) {
  uint8_t req[4] = { 0x05, DRIVER_ADDR, (uint8_t)(reg & 0x7F), 0 };
  req[3] = crc8(req, 3);

  uart_flush_rx();
  for (uint8_t i = 0; i < 4; i++) uart_tx(req[i]);

  // discard the 4 echoed bytes
  uint8_t tmp;
  for (uint8_t i = 0; i < 4; i++) {
    if (!uart_rx(tmp)) return false;
  }

  // receive the 8-byte reply
  uint8_t r[8];
  for (uint8_t i = 0; i < 8; i++) {
    if (!uart_rx(r[i])) return false;
  }

  if (r[0] != 0x05 || r[1] != 0xFF || r[2] != (reg & 0x7F)) return false;
  if (crc8(r, 7) != r[7]) return false;

  value = ((uint32_t)r[3] << 24) | ((uint32_t)r[4] << 16) |
          ((uint32_t)r[5] << 8)  |  (uint32_t)r[6];
  return true;
}

// ---- LED status ----
static void led_init() { DDRB |= (1 << DDB5); }
static void led_on()   { PORTB |=  (1 << PORTB5); }
static void led_off()  { PORTB &= ~(1 << PORTB5); }

static void led_blink_forever(uint16_t ms) {
  while (true) {
    led_on();  for (uint16_t i = 0; i < ms; i++) _delay_ms(1);
    led_off(); for (uint16_t i = 0; i < ms; i++) _delay_ms(1);
  }
}

int main() {
  led_init();

  // Driver disabled for now (EN is active LOW)
  DDRD  |= (1 << DDD7);
  PORTD |= (1 << PORTD7);

  uart_init();
  _delay_ms(500);   // let the TMC2209 power up

  uint32_t ioin = 0, ifcnt_before = 0, ifcnt_after = 0;

  // 1) Read IOIN and check chip version (bits 31..24 = 0x21 for TMC2209)
  if (!tmc_read(REG_IOIN, ioin)) led_blink_forever(80);        // fast blink: no comms
  if ((ioin >> 24) != 0x21)      led_blink_forever(500);       // slow blink: wrong chip

  // 2) Write test: IFCNT must increase by 1 after a successful write
  if (!tmc_read(REG_IFCNT, ifcnt_before)) led_blink_forever(80);
  if (!tmc_write(REG_GCONF, GCONF_PDN_DISABLE)) led_blink_forever(80);
  if (!tmc_read(REG_IFCNT, ifcnt_after)) led_blink_forever(80);
  if (((ifcnt_before + 1) & 0xFF) != (ifcnt_after & 0xFF)) led_blink_forever(80);

  led_on();   // solid on: read and write both verified
  while (true) {}
}