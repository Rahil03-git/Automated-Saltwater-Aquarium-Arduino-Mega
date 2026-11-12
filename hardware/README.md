# Hardware guide

## Parts

See [`bill_of_materials.csv`](bill_of_materials.csv). The report's priced parts total **5,350 BDT**; the whole project
(tank, animals, food) was estimated at about **8,000 BDT**. Extra parts the firmware needs (LCD, buzzer, relay, buttons,
resistors, supplies) were not priced in the report.

## Pin map (Arduino Mega 2560)

| Function | Pin | Origin | Notes |
|---|---|---|---|
| DS18B20 data | **D2** | report code | 4.7 kΩ pull-up between data and 5 V |
| pH sensor (PH-4502C) | **A0** | report code | analog output of the module |
| TDS sensor | **A1** | report code | analog output |
| Buzzer | **A11** | report code | used as a digital output, active buzzer |
| Ultrasonic TRIG / ECHO | D22 / D23 | rebuild | sensor above the water, pointing down |
| Feeder servo | D9 | rebuild | power from a separate 5 V supply |
| LED lighting (PWM) | D5 | rebuild | resistor (single LED) or logic-level MOSFET (strip) |
| Pump relay | D24 | rebuild | active-LOW relay module (`RELAY_ACTIVE_LEVEL`) |
| Heater relay *(optional)* | D25 | rebuild | `ENABLE_HEATER_RELAY` |
| Dosing pump *(optional)* | D26 | rebuild | `ENABLE_PH_DOSING` |
| LCD I2C SDA / SCL | D20 / D21 | rebuild | address `0x27` (some modules use `0x3F`) |
| Buttons FEED / LIGHT / PAGE | D30 / D31 / D32 | rebuild | to ground, internal pull-ups |

Change any pin in [`config.h`](../firmware/AutomatedSaltwaterAquarium/config.h).

## Wiring notes

- **Connect all grounds together.**
- **DS18B20:** VCC 5 V, GND, data to D2 with a 4.7 kΩ resistor between data and VCC.
- **PH-4502C:** 5 V module, `Po` to A0. Keep the probe wet; calibrate it (see the documentation, section 9).
- **TDS probe:** analog module to A1. Hobby probes read only about 0-1000 ppm and wear if powered continuously in water.
- **Ultrasonic sensor:** 5 V; mount above the water pointing down, clear of splashes; minimum distance about 2 cm.
- **Servo:** draws current peaks - use a separate 5 V supply with common ground.
- **Pump:** switch with a relay module on its own supply; add a flyback diode across the motor.
- **LCD:** I2C backpack on SDA (20) and SCL (21), 5 V.

## Safety

- Water and electricity: keep the Arduino, breadboard and relays dry and above the water line.
- **Never wire mains devices (heater, lights, filter) on a breadboard.** Use certified sockets, relays or solid-state relays and a GFCI/RCD.
- Test the pump in a bucket before connecting it to the tank, and always keep the dry-run protection enabled.
- Marine animals are sensitive: validate every sensor and calibration on the bench before trusting the controller.
