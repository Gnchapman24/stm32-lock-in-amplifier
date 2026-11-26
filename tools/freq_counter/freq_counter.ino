/*
 * freq_counter.ino - makeshift frequency counter on an Arduino UNO.
 *
 * Counts rising edges on UNO pin D5 (Timer1 external clock, T1) over a
 * 1-second gate and prints the frequency to Serial (115200 baud) once a second.
 *
 * Wiring:
 *   STM32 PB10 (Arduino D6 on the board)  ->  UNO D5
 *   STM32 GND                             ->  UNO GND   (common ground is required)
 *
 * The STM32 output is 3.3 V; the UNO reads it as HIGH (ATmega328P V_IH >= 3.0 V at 5 V).
 *
 * Expected on the STM32 after "sample start" on its serial console: ~100000 Hz.
 *
 * Calibration: the UNO's 16 MHz ceramic resonator is not accurate (often
 * +-0.1..0.5 %). If it runs slow, the 1 s gate is too long and the count reads
 * high. Measured 2026-11-25 against the STM32 on its HSE crystal (+-tens of ppm):
 * raw 100091.6..100092.0 Hz for a true 100000 Hz, so the gate is long by
 * 918 ppm. Both the raw and the corrected frequency are printed.
 * This constant belongs to one particular UNO and drifts with temperature.
 * It was derived from the HSE, so it can't be used to check the HSE itself.
 */

const unsigned long GATE_US = 1000000UL;   // 1 s gate
const double GATE_ERROR_PPM = 918.0;       // measured gate error of this UNO (see above)

volatile uint16_t overflows = 0;           // Timer1 is 16-bit; count overflows here

ISR(TIMER1_OVF_vect) {
  overflows++;
}

void setup() {
  Serial.begin(115200);
  pinMode(5, INPUT);                       // D5 = PD5 = T1

  noInterrupts();
  TCCR1A = 0;                              // normal mode
  TCNT1  = 0;
  TIFR1  = _BV(TOV1);                      // clear pending overflow
  TIMSK1 = _BV(TOIE1);                     // overflow interrupt on
  TCCR1B = _BV(CS12) | _BV(CS11) | _BV(CS10);  // external clock on T1, rising edge
  interrupts();

  Serial.println(F("freq counter ready: input on D5"));
}

void loop() {
  // Start a fresh count.
  noInterrupts();
  overflows = 0;
  TCNT1 = 0;
  TIFR1 = _BV(TOV1);
  interrupts();

  unsigned long t0 = micros();
  while (micros() - t0 < GATE_US) {
    // busy-wait; overflows are counted by the ISR
  }
  unsigned long elapsed = micros() - t0;

  // Read the count. Include an overflow that has happened but whose ISR has not run yet.
  noInterrupts();
  if (TIFR1 & _BV(TOV1)) {
    TIFR1 = _BV(TOV1);
    overflows++;
  }
  uint32_t edges = (uint32_t)overflows * 65536UL + TCNT1;
  interrupts();

  double hz_raw = (double)edges * 1000000.0 / (double)elapsed;
  double hz_cal = hz_raw / (1.0 + GATE_ERROR_PPM * 1e-6);

  Serial.print(F("edges="));
  Serial.print(edges);
  Serial.print(F("  f_raw="));
  Serial.print(hz_raw, 1);
  Serial.print(F(" Hz  f_cal="));
  Serial.print(hz_cal, 1);
  Serial.println(F(" Hz"));
}
