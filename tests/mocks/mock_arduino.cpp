#include "Arduino.h"
#include <stdarg.h>

SimHW sim;
uint8_t sim_eeprom[4096];
MockSerial Serial;

void SimHW::reset() {
  nowMs = 0;
  for (int i = 0; i < 80; i++) { mode[i] = INPUT; level[i] = LOW; analogIn[i] = 0; pwm[i] = 0; highWrites[i] = 0; }
  tempC = 25.0f; tempConnected = true;
  levelCm = 5.0f; levelConnected = true;
  servoLog.clear(); servoAttachCount = 0;
  for (int r = 0; r < 2; r++) { memset(lcd[r], ' ', 16); lcd[r][16] = 0; }
  serialOut.clear(); serialIn.clear(); serialInPos = 0;
}

void pinMode(uint8_t pin, uint8_t m) {
  sim.mode[pin] = m;
  if (m == INPUT_PULLUP) sim.level[pin] = HIGH;
}
void digitalWrite(uint8_t pin, uint8_t v) {
  sim.level[pin] = v;
  if (v == HIGH) sim.highWrites[pin]++;
}
int digitalRead(uint8_t pin) { return sim.level[pin]; }
int analogRead(uint8_t pin) { return sim.analogIn[pin]; }
void analogWrite(uint8_t pin, int v) { sim.pwm[pin] = v; }
uint32_t millis() { return sim.nowMs; }
uint32_t micros() { return sim.nowMs * 1000u; }
void delay(uint32_t ms) { sim.nowMs += ms; }
void delayMicroseconds(unsigned int) {}
unsigned long pulseIn(uint8_t, uint8_t, unsigned long) {
  if (!sim.levelConnected) return 0;
  return (unsigned long)(sim.levelCm * 58.0f);          // echo time for the simulated distance
}
char* dtostrf(double value, signed char width, unsigned char prec, char* out) {
  snprintf(out, 16, "%*.*f", (int)width, (int)prec, value);
  return out;
}

int MockSerial::available() { return (int)(sim.serialIn.size() - sim.serialInPos); }
int MockSerial::read() { return available() > 0 ? (unsigned char)sim.serialIn[sim.serialInPos++] : -1; }
size_t MockSerial::print(const char* s) { sim.serialOut += s; return strlen(s); }
size_t MockSerial::print(char c) { sim.serialOut += c; return 1; }
size_t MockSerial::print(unsigned char v, int) { char b[8]; snprintf(b, 8, "%u", v); return print(b); }
size_t MockSerial::print(int v) { char b[16]; snprintf(b, 16, "%d", v); return print(b); }
size_t MockSerial::print(unsigned int v) { char b[16]; snprintf(b, 16, "%u", v); return print(b); }
size_t MockSerial::print(long v) { char b[24]; snprintf(b, 24, "%ld", v); return print(b); }
size_t MockSerial::print(unsigned long v) { char b[24]; snprintf(b, 24, "%lu", v); return print(b); }
size_t MockSerial::print(double v, int digits) { char b[32]; snprintf(b, 32, "%.*f", digits, v); return print(b); }
