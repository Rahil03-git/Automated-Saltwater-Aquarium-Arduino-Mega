// buttons.h - three push buttons (to ground, internal pull-ups) for manual override.
#ifndef AQ_BUTTONS_H
#define AQ_BUTTONS_H

#include <stdint.h>

enum ButtonEvent : uint8_t {
  BTN_NONE = 0,
  BTN_FEED,      // feed now
  BTN_LIGHT,     // cycle light mode AUTO -> ON -> OFF
  BTN_PAGE,      // next LCD page
  BTN_MUTE       // long press on the page button: silence the buzzer
};

namespace Buttons {
  void begin();
  ButtonEvent poll();        // returns at most one event per call
}

#endif
