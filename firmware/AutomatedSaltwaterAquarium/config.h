// =============================================================================
//  Automated Saltwater Aquarium - configuration
//  Arduino Mega 2560  |  all pins, limits, schedules and timings live here.
//  Edit this file to match your wiring and your tank - no other file needs to change.
// =============================================================================
#ifndef AQ_CONFIG_H
#define AQ_CONFIG_H

#include <stdint.h>

// -----------------------------------------------------------------------------
//  1. Safe-range profile
//     PROFILE_REPORT : limits taken from the code in the project report
//                      (TDS 300-400 ppm, 28-35 C, pH 6.32-6.80) - demonstration water
//     PROFILE_MARINE : typical marine-aquarium limits (pH 8.0-8.4, 24-28 C).
//                      The TDS check is disabled because hobby TDS probes read only
//                      0-1000 ppm, far below seawater (about 35,000 ppm).
// -----------------------------------------------------------------------------
#define PROFILE_REPORT 1
#define PROFILE_MARINE 2
#ifndef ACTIVE_PROFILE
#define ACTIVE_PROFILE PROFILE_REPORT
#endif

#if ACTIVE_PROFILE == PROFILE_REPORT
  #define TEMP_MIN_C        28.0f
  #define TEMP_MAX_C        35.0f
  #define PH_MIN            6.32f
  #define PH_MAX            6.80f
  #define TDS_MIN_PPM       300.0f
  #define TDS_MAX_PPM       400.0f
  #define TDS_ALARM_ENABLED 1
#elif ACTIVE_PROFILE == PROFILE_MARINE
  #define TEMP_MIN_C        24.0f
  #define TEMP_MAX_C        28.0f
  #define PH_MIN            8.00f
  #define PH_MAX            8.40f
  #define TDS_MIN_PPM       0.0f
  #define TDS_MAX_PPM       1000.0f
  #define TDS_ALARM_ENABLED 0
#else
  #error "ACTIVE_PROFILE must be PROFILE_REPORT or PROFILE_MARINE"
#endif

// -----------------------------------------------------------------------------
//  2. Pins (Arduino Mega 2560)
//     ONE_WIRE_BUS, PH_SENSOR_PIN, TDS_SENSOR_PIN and BUZZER_PIN are the pins used
//     in the original project code. The others were chosen when this firmware was
//     rebuilt - change them to match your wiring.
// -----------------------------------------------------------------------------
#define ONE_WIRE_BUS         2    // DS18B20 data (4.7 k pull-up to 5 V)
#define PH_SENSOR_PIN        A0   // pH-4502C analog output
#define TDS_SENSOR_PIN       A1   // TDS probe analog output
#define BUZZER_PIN           A11  // active buzzer (used as a digital output)

#define ULTRASONIC_TRIG_PIN  22   // HC-SR04 trigger
#define ULTRASONIC_ECHO_PIN  23   // HC-SR04 echo
#define FEEDER_SERVO_PIN     9    // fish-feeder servo signal
#define LED_PWM_PIN          5    // aquarium LEDs (PWM, via resistor or MOSFET)
#define PUMP_RELAY_PIN       24   // water-pump relay module input
#define HEATER_RELAY_PIN     25   // optional heater relay (see ENABLE_HEATER_RELAY)
#define DOSING_PUMP_PIN      26   // optional pH dosing pump (see ENABLE_PH_DOSING)
#define BTN_FEED_PIN         30   // push button to ground: feed now
#define BTN_LIGHT_PIN        31   // push button to ground: light mode AUTO / ON / OFF
#define BTN_PAGE_PIN         32   // push button to ground: next LCD page, long press = mute

#define LCD_I2C_ADDRESS      0x27 // many I2C backpacks use 0x27, some use 0x3F
#define LCD_COLUMNS          16
#define LCD_ROWS             2

// Relay modules are usually "active LOW" (input LOW = relay ON).
#define RELAY_ACTIVE_LEVEL   LOW

// -----------------------------------------------------------------------------
//  3. Sensors and calibration defaults (values from the project report)
// -----------------------------------------------------------------------------
#define ADC_VREF             5.0f     // Arduino reference voltage
#define ADC_STEPS            1024.0f

#define PH_NEUTRAL_VOLTAGE   2.5f     // sensor voltage at pH 7
#define PH_SLOPE_V_PER_PH    -3.0f    // value used in the report - CALIBRATE (see docs)
#define PH_OFFSET            0.0f     // calibrationOffset in the report

#define TDS_SAMPLE_COUNT     30       // SCOUNT in the report
#define TDS_SAMPLE_MS        40       // one sample every 40 ms
#define TDS_REPORT_MS        1000     // new TDS value every second
#define PH_SAMPLE_COUNT      10
#define PH_SAMPLE_MS         20

#define TEMP_REQUEST_MS      1000     // DS18B20 conversion every second
#define TEMP_CONVERSION_MS   800      // 12-bit conversion needs 750 ms
#define TEMP_DEFAULT_C       25.0f    // used for TDS compensation until a reading exists

#define LEVEL_INTERVAL_MS    2000     // water-level measurement cycle
#define LEVEL_PING_GAP_MS    60       // spacing between the 5 pings of one cycle
#define LEVEL_VALID_MIN_CM   2.0f     // HC-SR04 cannot measure closer than ~2 cm
#define LEVEL_VALID_MAX_CM   150.0f

// -----------------------------------------------------------------------------
//  4. Water level control. The ultrasonic sensor looks DOWN at the water surface,
//     so a SMALLER distance means MORE water.
// -----------------------------------------------------------------------------
#define LEVEL_FULL_CM        5.0f     // pump stops when distance <= this
#define LEVEL_LOW_CM         9.0f     // pump starts when distance >= this
#define LEVEL_ALARM_LOW_CM   12.0f    // alarm: water far too low (top-up failed)
#define LEVEL_OVERFLOW_CM    3.0f     // alarm + pump inhibited: about to overflow

#define PUMP_MAX_RUN_MS      90000UL  // dry-run / leak protection
#define PUMP_COOLDOWN_MS     600000UL // rest between top-up cycles
#define PUMP_MAX_FAILS       3        // failed cycles before the pump is locked out

// -----------------------------------------------------------------------------
//  5. Feeding schedule (minutes after midnight)
// -----------------------------------------------------------------------------
static const uint16_t FEED_TIMES_MIN[] = { 8 * 60, 18 * 60 };   // 08:00 and 18:00
#define FEED_TIME_COUNT      (sizeof(FEED_TIMES_MIN) / sizeof(FEED_TIMES_MIN[0]))
#define MAX_FEEDS_PER_DAY    4        // scheduled + manual (prevents overfeeding)
#define MIN_FEED_GAP_MIN     60       // minimum gap between two feeds
#define FEED_SERVO_CLOSED    0        // servo angles
#define FEED_SERVO_OPEN      90
#define FEED_OPEN_MS         700      // time the hopper stays open
#define FEED_RETURN_MS       600      // time allowed to return before detaching
#ifndef BOOT_FEED_LOCKOUT_MS
#define BOOT_FEED_LOCKOUT_MS 60000UL  // no scheduled feed right after power-up
#endif

// -----------------------------------------------------------------------------
//  6. Lighting schedule (day-night cycle with sunrise / sunset ramps)
// -----------------------------------------------------------------------------
#define SUNRISE_MIN          (7 * 60)     // 07:00 lights start to rise
#define SUNSET_MIN           (18 * 60)    // 18:00 lights start to fade
#define LIGHT_RAMP_MIN       60           // length of sunrise / sunset
#define LIGHT_DAY_PERCENT    100
#define LIGHT_NIGHT_PERCENT  0

// -----------------------------------------------------------------------------
//  7. Clock. There is no RTC in the original bill of materials, so the clock is
//     software-based: set it with the serial command  TIME HH:MM  after power-up.
//     TIME_SCALE > 1 speeds up the day for demonstrations (60 = 1 s per minute).
// -----------------------------------------------------------------------------
#define DEFAULT_START_MINUTE (12 * 60)    // 12:00 at power-up
#ifndef TIME_SCALE
#define TIME_SCALE           1
#endif

// -----------------------------------------------------------------------------
//  8. Alarms and user interface
// -----------------------------------------------------------------------------
#define ALARM_CONFIRM_TICKS  3            // seconds a fault must persist
#define ALARM_MUTE_MS        600000UL     // buzzer silence after a long press (10 min)
#define SERIAL_BAUD          9600
#define CONTROL_TICK_MS      1000
#define LOG_INTERVAL_MS      5000
#define LCD_REFRESH_MS       500
#define LCD_PAGE_MS          4000
#define BUTTON_DEBOUNCE_MS   40
#define BUTTON_LONG_MS       1500

// -----------------------------------------------------------------------------
//  9. OPTIONAL hardware described in the report but NOT in its priced parts list.
//     Both are OFF by default.
// -----------------------------------------------------------------------------
#ifndef ENABLE_HEATER_RELAY
#define ENABLE_HEATER_RELAY  0            // heater on/off with hysteresis (fail-safe off)
#endif
#define HEATER_HYSTERESIS_C  0.5f
#define HEATER_TARGET_C      ((TEMP_MIN_C + TEMP_MAX_C) / 2.0f)

#ifndef ENABLE_PH_DOSING
#define ENABLE_PH_DOSING     0            // pH-up dosing pulses (use with great care)
#endif
#define DOSING_PULSE_MS      2000UL
#define DOSING_MIN_GAP_MIN   30
#define DOSING_MAX_PER_DAY   5

#endif  // AQ_CONFIG_H
