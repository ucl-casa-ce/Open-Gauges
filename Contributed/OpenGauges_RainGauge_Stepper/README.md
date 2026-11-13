# RainGauge: stepper edition

*Part of the **Open Gauges** project: open, 3D-printable dials for live environmental data.*

![RainGauge, assembled, pointer at 6.4 mm](images/raingauge_hero.jpg)

A wall-mounted dial that shows how much rain has fallen today at your weather station. An ESP32-C3 Super Mini reads the day's rainfall from the weeWX loop packet over MQTT and turns a 28BYJ-48 stepper over a 0–50 mm scale, from DAMP through WET and SOAKED to DELUGE.

It shares the case, bezel, pointer, magnet carrier and acrylic window with the [WindGauge stepper edition](../OpenGauges_WindGauge_Stepper/), and the board, wiring and firmware layout with the [WindSpeedGauge](../OpenGauges_WindSpeedGauge_Stepper/), [PressureGauge](../OpenGauges_PressureGauge_Stepper/) and [TemperatureGauge](../OpenGauges_TemperatureGauge_Stepper/). Only the dial and the firmware settings are different.

- **Stretched 270° scale:** 0 mm bottom-left, 50 mm bottom-right, 17 mm at 12 o'clock. The low end is opened out so a few millimetres are easy to read: 0–2 mm takes 5 times the space per millimetre of 10–50 mm, and 2–10 mm twice the space. The pointer never crosses the gap at the bottom.
- **Resets itself:** the station zeroes the day's rain at midnight, so the pointer returns to 0 on its own.
- **Self-homing:** the same magnet and KY-003 hall sensor as the wind gauge. The sketch measures the motor's steps per revolution itself and re-zeros every six hours.
- **Backlash compensation:** every move finishes clockwise, so the gearbox play in the 28BYJ-48 doesn't shift the reading.
- **Low power when idle:** the motor coils switch off between moves, and between showers the pointer doesn't move at all.

---

## How it works

Your weather station publishes its weeWX loop packet (JSON) to an MQTT broker. The ESP32-C3 subscribes, picks out today's rainfall (`dayRain_mm`, `dayRain_cm` or `dayRain_in`, converted to mm) and moves the pointer. Changes under 0.1 mm are ignored; a Davis bucket tips at 0.2 mm, so every tip shows.

## Repository layout

```
print/stl/        rainfall dial, oriented for printing (STL)
print/3mf/        the same dial as 3MF
print/flat-dial/  flat two-colour dial (3MF with both parts + separate STLs)
cad/              Fusion 360 archive (.f3d) with all the stepper gauge dials, and STEP files
firmware/         Arduino sketch for the ESP32-C3 Super Mini
images/           render used in this README
```

The case, bezel, pointer, magnet carrier and acrylic window are shared with the wind gauge: print them from [`../OpenGauges_WindGauge_Stepper/print/`](../OpenGauges_WindGauge_Stepper/print/). Their CAD is in [`../OpenGauges_WindGauge_Stepper/cad/`](../OpenGauges_WindGauge_Stepper/cad/).

## Bill of materials

### Printed parts (PLA, 0.2 mm layers, 3 walls, 15 % infill, no supports)

| Part | File | Orientation |
|---|---|---|
| Case | `WG-01_Case_back-down` (WindGauge folder) | Back down |
| Dial, raised (choose one) | `RG-02_Rainfall_dial_face-up_M600` | Face up, colour change at 2.2 mm |
| Dial, flat two-colour (choose one) | `RG-02F_Rainfall_flat_dial_2colour_Bambu.3mf` | Face down |
| Bezel | `WG-03_Bezel_front-down` (WindGauge folder) | Front down |
| Pointer | `WG-04_Pointer` (WindGauge folder) | Underside down |
| Magnet carrier | `WG-05_Magnet_carrier` (WindGauge folder) | Underside down |

The bezel needs at least 205 mm in one direction: it fits a Prusa MK4 and Bambu X1, P1 and A1, but not an A1 mini or Prusa MINI.

### Printing notes

- **Raised dial:** print in white and add a filament change (M600) at the **2.2 mm** layer so the marks print black.
- **Flat dial:** the black marks are inlaid 0.2 mm (one layer) and the dial prints face down. On a Bambu with AMS, open the 3MF and set *Dial white* / *Marks black*. On a Prusa MK4 without an MMU, use a two-extruder *Single Extruder Multi Material* profile with `M600` as the tool-change G-code and a small wipe tower (see the WindGauge guide).
- The dial's notch sits at 12 o'clock (17 mm) and lines up with the case's N rib, as on the wind gauge.

### Bought parts

| Item | Notes |
|---|---|
| ESP32-C3 Super Mini | Wi-Fi; native USB |
| 28BYJ-48 stepper (5 V) + ULN2003 driver | 5 V version |
| KY-003 hall sensor module (A3144) | |
| Neodymium magnet, Ø5 × up to 3 mm | glued into the carrier |
| WS2812B LED strip, 10 mm wide | 84 LEDs in the sketch; change `NUM_LEDS` to match yours |
| Clear acrylic disc, Ø190 × 3 mm | laser cut from the WindGauge `cad/Acrylic_190mm_x3_disc.dxf` |
| 2 × M3 × 6 pan-head screws | motor ears |
| Jumper wires, USB-C cable, 5 V USB supply | a 2 A supply gives the LEDs headroom |

## Wiring

Identical to the wind direction, wind speed, pressure and temperature gauges: see the [wiring diagram](../OpenGauges_WindGauge_Stepper/docs/Wiring_ESP32C3.png).

| ESP32-C3 Super Mini | Connects to |
|---|---|
| GPIO4 / GPIO3 / GPIO1 / GPIO0 | ULN2003 IN1 / IN2 / IN3 / IN4 |
| GPIO7 | KY-003 S |
| GPIO10 | WS2812B DIN |
| 5V | ULN2003 +, KY-003 +, LED 5V |
| GND | all grounds |

GPIO2, GPIO8 and GPIO9 are strapping pins on the C3, so they're left free.

Everything runs from USB through the Super Mini's 5V pin, wired directly. Optional extras for more margin:

- **Divider on the hall sensor signal:** the KY-003 pulls its output up to 5 V, and the ESP32-C3 is a 3.3 V chip. A 10 kΩ / 20 kΩ divider on the signal keeps GPIO7 within spec. Without it, the module's own 10 kΩ pull-up limits the current, and it works in practice.
- **Power the hall sensor only while homing:** wire KY-003 + to GPIO5 instead of 5V. The sketch switches it on just for homing, which saves a few mA (useful on battery). The A3144 is rated from 4.5 V, so check that yours triggers at 3.3 V; most do.
- **LED strip:** a 330 Ω resistor on LED data and a 1000 µF capacitor across the strip.

## Firmware

[`firmware/RainGauge_ESP32C3/RainGauge_ESP32C3.ino`](firmware/RainGauge_ESP32C3/RainGauge_ESP32C3.ino)

1. Arduino IDE: install **esp32 by Espressif Systems** (v3.x) and select **ESP32C3 Dev Module**.
2. Tools menu: set **USB CDC On Boot** to **Enabled** for the Serial Monitor.
3. Libraries: **AccelStepper**, **PubSubClient**, **ArduinoJson** (v7), **Adafruit NeoPixel**.
4. Edit the settings at the top of the sketch and upload.

| Setting | What it does |
|---|---|
| `ssid`, `password` | your Wi-Fi |
| `mqtt_server`, `mqtt_port`, `rain_topic` | your broker and topic: a weeWX loop packet (JSON) or a plain number in mm (or cm / inches if the topic ends in `_cm` / `_in`) |
| `bareKeyUnits` | units of a `dayRain` key with no unit suffix: `mm`, `cm` or `in` (weeWX in US units sends inches) |
| `scaleMm`, `scaleDeg` | the dial's stretched scale: where 0, 2, 10 and 50 mm sit, in degrees from 12 o'clock. Change these only if you redesign the dial |
| `homeOffsetDeg` | fine-tunes 17 mm at 12 o'clock: pointer short → increase, overshoots → decrease |
| `backlashDeg` | gearbox play to take up on anticlockwise moves (typically 2–6°) |
| `NUM_LEDS`, `LED_BRIGHTNESS` | LED count and backlight brightness |

### Start-up

The ring shows how far start-up has got, so you can follow it without the Serial Monitor:

| Ring colour | Stage |
|---|---|
| Red | sketch started |
| Blue | connecting to Wi-Fi |
| Yellow | homing and measuring one revolution (motor turning) |
| Purple | self-test sweep: 0 → 50 → 0 mm |
| White | ready |

Wi-Fi connects before the motor runs, so the two biggest power draws never overlap.

## Troubleshooting

| Symptom | Fix |
|---|---|
| Ring stays red or dark | Sketch not running: unplug the USB and plug it back in (after an upload the C3 can stay in download mode). |
| Ring stays blue | Wi-Fi not connecting: check the name and password, and that it's a 2.4 GHz network. Transmit power is set low (8.5 dBm) for the Super Mini's small antenna, so try it closer to the router. |
| Ring yellow, motor turning without stopping | Hall sensor not triggering: flip the magnet and keep the gap around 1.5 mm. |
| Ring yellow, motor still | IN1–IN4 wiring or no 5 V on the ULN2003. |
| No Serial Monitor output | USB CDC On Boot must be Enabled; press RST with the monitor open. |
| Reading 25 times too low or high | Units mismatch: your feed's rain key has no unit suffix. Set `bareKeyUnits` to `in` or `mm`. |
| Pointer a little short after the midnight reset | Increase `backlashDeg`. |

## CAD

`cad/WeatherGauges_Stepper.f3d` is the full parametric Fusion 360 model, with the shared case parts and every stepper gauge dial: wind direction, wind speed, barometer, temperature and rainfall. `cad/step/` has the rainfall dials as STEP files.

## Licence

- Design files (`print/`, `cad/`, `images/`): [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)
- Firmware (`firmware/`): [MIT](firmware/LICENSE)

See [`LICENSE.md`](LICENSE.md).

## Credits

Designed and built by **Andy Hudson-Smith**, [digitalurban.org](https://www.digitalurban.org).
