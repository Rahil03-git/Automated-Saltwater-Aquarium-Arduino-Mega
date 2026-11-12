#include <Arduino.h>
#include "config.h"
#include "buttons.h"

namespace {
struct Btn {
  uint8_t pin;
  bool stable;        // true = pressed (debounced)
  bool lastRaw;
  uint32_t changedAt;
  uint32_t pressedAt;
};
Btn btns[3] = {
  { BTN_FEED_PIN,  false, false, 0, 0 },
  { BTN_LIGHT_PIN, false, false, 0, 0 },
  { BTN_PAGE_PIN,  false, false, 0, 0 },
};
}

namespace Buttons {

void begin() {
  for (uint8_t i = 0; i < 3; i++) pinMode(btns[i].pin, INPUT_PULLUP);
}

ButtonEvent poll() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < 3; i++) {
    bool raw = (digitalRead(btns[i].pin) == LOW);        // pressed = LOW
    if (raw != btns[i].lastRaw) {
      btns[i].lastRaw = raw;
      btns[i].changedAt = now;
    }
    if (now - btns[i].changedAt >= BUTTON_DEBOUNCE_MS && raw != btns[i].stable) {
      btns[i].stable = raw;
      if (raw) {                                          // pressed
        btns[i].pressedAt = now;
        if (i == 0) return BTN_FEED;
        if (i == 1) return BTN_LIGHT;
      } else if (i == 2) {                                // page button released
        return (now - btns[i].pressedAt >= BUTTON_LONG_MS) ? BTN_MUTE : BTN_PAGE;
      }
    }
  }
  return BTN_NONE;
}

}  // namespace Buttons
