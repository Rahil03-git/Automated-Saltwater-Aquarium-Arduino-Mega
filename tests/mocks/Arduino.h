// Minimal Arduino API mock so the firmware can be compiled and simulated on a PC.
#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

// Arduino Mega: A0 = pin 54 ... A15 = pin 69
enum { A0 = 54, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15 };

#define F(x) (x)
typedef uint8_t byte;

// ---------------------------------------------------------------------------------
//  Simulated hardware state (set by tests, read by the mocked Arduino functions)
// ---------------------------------------------------------------------------------
struct SimHW {
  uint32_t nowMs;
  int      mode[80];
  int      level[80];        // last digital level written / pulled-up input level
  int      analogIn[80];     // value returned by analogRead()
  int      pwm[80];          // last analogWrite() value
  int      highWrites[80];   // number of HIGH writes per pin
  // sensors
  float    tempC;      bool tempConnected;
  float    levelCm;    bool levelConnected;
  // devices
  std::string servoLog;      // e.g. "A90,W0,D" = attach, write 90, write 0, detach
  int      servoAttachCount;
  char     lcd[2][17];
  std::string serialOut;
  std::string serialIn;
  size_t   serialInPos;
  void reset();
  void setPhVoltage(float v)  { analogIn[A0] = (int)(v / 5.0f * 1024.0f + 0.5f); }
  void setTdsVoltage(float v) { analogIn[A1] = (int)(v / 5.0f * 1024.0f + 0.5f); }
};
extern SimHW sim;
extern uint8_t sim_eeprom[4096];

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int  digitalRead(uint8_t pin);
int  analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int value);
uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(unsigned int us);
unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout);
char* dtostrf(double value, signed char width, unsigned char prec, char* out);

class MockSerial {
 public:
  void begin(unsigned long) {}
  int available();
  int read();
  size_t print(const char* s);
  size_t print(char c);
  size_t print(unsigned char v, int = 10);
  size_t print(int v);
  size_t print(unsigned int v);
  size_t print(long v);
  size_t print(unsigned long v);
  size_t print(double v, int digits = 2);
  size_t println() { return print("\n"); }
  template <typename T> size_t println(T v) { size_t n = print(v); return n + print("\n"); }
  size_t println(double v, int digits) { size_t n = print(v, digits); return n + print("\n"); }
};
extern MockSerial Serial;

#endif
