// cli.h - serial command line (9600 baud). Type HELP in the Serial Monitor.
#ifndef AQ_CLI_H
#define AQ_CLI_H

#include <stdint.h>

enum CmdType : uint8_t {
  CMD_NONE = 0, CMD_HELP, CMD_STATUS, CMD_PROFILE, CMD_FEED,
  CMD_LIGHT_AUTO, CMD_LIGHT_ON, CMD_LIGHT_OFF,
  CMD_PUMP_AUTO, CMD_PUMP_ON, CMD_PUMP_OFF, CMD_PUMP_RESET,
  CMD_TIME, CMD_MUTE,
  CMD_CAL_PH7, CMD_CAL_PH, CMD_CAL_TDS, CMD_CAL_RESET,
  CMD_UNKNOWN
};

struct Command {
  CmdType type;
  float   value;       // CALPH <buffer pH>, CALTDS <ppm>
  uint8_t hour;        // TIME HH:MM
  uint8_t minute;
};

namespace Cli {
  void begin();
  bool poll(Command& out);      // true when a complete line has been parsed
}

#endif
