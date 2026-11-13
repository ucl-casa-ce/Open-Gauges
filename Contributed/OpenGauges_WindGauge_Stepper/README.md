# WindGauge: stepper edition

*Part of the **Open Gauges** project: open, 3D-printable dials for live environmental data.*

![WindGauge, assembled, pointer at 300°](images/windgauge_hero.jpg)

A 205 mm wall-mounted dial that shows live wind direction from your weather station. An ESP32-C3 Super Mini reads the bearing from an MQTT feed over Wi-Fi and turns a 28BYJ-48 stepper. A hall sensor finds North on every start-up, and an LED strip lights the dial from behind.

- **Prints on a 220 mm-class bed:** five parts, no supports, largest part 204.8 mm.
- **Self-homing:** a magnet on the pointer carrier passes a KY-003 hall sensor at North. The sketch measures the motor's steps per revolution itself and re-zeros every six hours.
- **Screw-on bezel:** M200 × 4 thread with print clearance, holding a 190 mm × 3 mm acrylic window.
- **Clip-in parts:** dial, LED strip, driver board and breadboard all clip in. One screw hangs it on the wall.
- **Two dial options:** raised marks (single filament change) or a flat two-colour inlay (Bambu AMS or manual swaps on a Prusa MK4).

📘 **Full illustrated build guide:** [`docs/WindGauge_Stepper_HowTo.pdf`](docs/WindGauge_Stepper_HowTo.pdf)

---

## How it works

Your weather station publishes wind direction (degrees) to an MQTT broker. The ESP32-C3 subscribes and turns the stepper by the shortest path to that bearing. It uses the same board, wiring and firmware layout as the [PressureGauge](../OpenGauges_PressureGauge_Stepper/) and [TemperatureGauge](../OpenGauges_TemperatureGauge_Stepper/). The pointer sits on a printed **magnet carrier** on the motor shaft; a 5 mm magnet in the carrier passes over the hall sensor at North.

![Exploded view](images/windgauge_exploded.jpg)

| Inside the case (dial removed) | From behind: keyhole and USB opening |
|---|---|
| ![Interior](images/windgauge_interior.jpg) | ![Rear](images/windgauge_rear.jpg) |

## Repository layout

```
print/stl/        printed parts, oriented for printing (STL)
print/3mf/        the same parts as 3MF
print/flat-dial/  flat two-colour dial (3MF with both parts + separate STLs)
cad/              Fusion 360 archive (.f3d), STEP files, acrylic window DXF
firmware/         Arduino sketch for the ESP32-C3 Super Mini
docs/             how-to guide (PDF + editable Word), wiring diagram
images/           renders used in this README
```

## Bill of materials

### Printed parts (PLA, 0.2 mm layers, 3 walls, 15 % infill, no supports)

| Part | File | Orientation |
|---|---|---|
| Case | `WG-01_Case_back-down` | Back down |
| Dial, raised (choose one) | `WG-02_Dial_face-up_M600` | Face up, colour change at 2.2 mm |
| Dial, flat two-colour (choose one) | `WG-02F_Flat_dial_2colour_Bambu.3mf` | Face down |
| Bezel | `WG-03_Bezel_front-down` | Front down |
| Pointer | `WG-04_Pointer` | Underside down |
| Magnet carrier | `WG-05_Magnet_carrier` | Underside down |

The bezel needs at least 205 mm in one direction: it fits a Prusa MK4 and Bambu X1, P1 and A1, but not an A1 mini or Prusa MINI.

### Bought parts

| Item | Notes |
|---|---|
| ESP32-C3 Super Mini (with headers) | Wi-Fi; native USB-C; 3.3 V logic |
| Mini breadboard, 170 tie-points | ~46 × 35 × 9.4 mm; side tabs clip into the tray notches |
| 28BYJ-48 stepper (5 V) + ULN2003 driver | 5 V version |
| KY-003 hall sensor module (A3144) | |
| Neodymium magnet, Ø5 × up to 3 mm | glued into the carrier |
| WS2812B LED strip, 10 mm wide, ≤ 2.2 mm thick | ~0.6 m; bare or IP30 (not silicone-sleeved) |
| Clear acrylic disc, Ø190 × 3 mm | laser cut from `cad/Acrylic_190mm_x3_disc.dxf` |
| 2 × M3 × 6 pan-head screws | motor ears |
| Jumper wires, USB-C cable, 5 V USB supply | a 2 A supply gives the LEDs headroom |
| 1 × wall screw | head ≤ 9.5 mm across, ≤ 3.5 mm thick |

## Printing notes

- **Raised dial:** print in white and add a filament change (M600) at the **2.2 mm** layer so the marks print black.
- **Flat dial:** the black marks are inlaid 0.6 mm and the dial prints face down. On a Bambu with AMS, open the 3MF and set *Dial white* / *Marks black*. On a Prusa MK4 without an MMU, use a two-extruder *Single Extruder Multi Material* profile with `M600` as the tool-change G-code and a small wipe tower (see the guide).
- The pointer and magnet carrier push on until they stop. Those stops keep the pointer 1 mm above the dial and 1 mm below the acrylic.

![Dial options](images/windgauge_dials.png)

## Assembly (short version)

1. Test-fit the dial and bezel before adding electronics.
2. Drop the 28BYJ-48 into its pocket (wire cover down) and fix it with 2 × M3 × 6.
3. Snap the ULN2003 into its cradle and plug in the motor.
4. Press the breadboard into the bottom tray (tabs into the notches) and plug the ESP32-C3 in at the wall end, USB-C towards the wall.
5. Lay the A3144 flat on its shelf and snap the KY-003 in.
6. Check the magnet's polarity (KY-003 LED lights), glue it into the carrier, and push the carrier onto the shaft.
7. Clip the LED strip into the channel, starting at the wire gap at 5 o'clock.
8. Wire up, upload the sketch and watch it home.
9. Clip the dial in (notch to the N rib), then push on the pointer.
10. Clip the acrylic into the bezel and screw it on.
11. Hang it: the keyhole puts the screw 67 mm above the dial centre.

## Wiring

![Wiring diagram](docs/Wiring_ESP32C3.png)

Identical to the PressureGauge and TemperatureGauge.

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

[`firmware/WindGauge_ESP32C3/WindGauge_ESP32C3.ino`](firmware/WindGauge_ESP32C3/WindGauge_ESP32C3.ino)

1. Arduino IDE: install **esp32 by Espressif Systems** (v3.x) and select **ESP32C3 Dev Module**.
2. Tools menu: set **USB CDC On Boot** to **Enabled** for the Serial Monitor.
3. Libraries: **AccelStepper**, **PubSubClient**, **ArduinoJson** (v7), **Adafruit NeoPixel**.
4. Edit the settings at the top of the sketch and upload.

| Setting | What it does |
|---|---|
| `ssid`, `password` | your Wi-Fi |
| `mqtt_server`, `mqtt_port`, `wind_topic` | your broker and topic: plain degrees (e.g. `247`) or a weeWX loop packet (JSON) with `windDir` |
| `northOffsetDeg` | fine-tunes North: pointer short of N → increase, overshoots → decrease |
| `backlashDeg` | gearbox play to take up on anticlockwise moves (typically 2–6°) |
| `moveDeadbandDeg` | ignore changes smaller than this (2° in the sketch), so the pointer doesn't twitch |
| `NUM_LEDS`, `LED_BRIGHTNESS` | LED count and backlight brightness (lower it if the board resets) |

### Start-up

The ring shows how far start-up has got, so you can follow it without the Serial Monitor:

| Ring colour | Stage |
|---|---|
| Red | sketch started |
| Blue | connecting to Wi-Fi |
| Yellow | homing and measuring one revolution (motor turning) |
| Purple | self-test sweep: N → S → N |
| White | ready |

Wi-Fi connects before the motor runs, so the two biggest power draws never overlap. Calm readings (no direction) are ignored, so the pointer holds its last bearing. The motor coils switch off between moves, and the gauge re-homes on the magnet every six hours.

## Troubleshooting

| Symptom | Fix |
|---|---|
| Ring stays red or dark | Sketch not running: unplug the USB and plug it back in (after an upload the C3 can stay in download mode). |
| Ring stays blue | Wi-Fi not connecting: check the name and password, and that it's a 2.4 GHz network. Transmit power is set low (8.5 dBm) for the Super Mini's small antenna, so try it closer to the router. |
| Ring yellow, motor turning without stopping | Hall sensor not triggering: flip the magnet; the gap should be ~1.5 mm. |
| Ring yellow, motor still | IN1–IN4 wiring or no 5 V on the ULN2003. |
| Motor buzzes, doesn't turn | IN1–IN4 out of order: GPIO4, GPIO3, GPIO1, GPIO0 to IN1–IN4. |
| No Serial Monitor output | USB CDC On Boot must be Enabled; press RST with the monitor open. |
| Board resets when LEDs light | Lower `LED_BRIGHTNESS` or use a stronger supply. |
| Pointer a degree or two off after anticlockwise moves | Increase `backlashDeg`. |
| Pointer rubs | Push the carrier and pointer fully home; both have hard stops. |

## CAD

`cad/` has the full parametric Fusion 360 model (`.f3d`) and STEP files. The breadboard tray, notch width, clearances and dial geometry are user parameters, so you can adapt them to your parts.

## Licence

- Design files (`print/`, `cad/`, `docs/`, `images/`): [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)
- Firmware (`firmware/`): [MIT](firmware/LICENSE)

See [`LICENSE.md`](LICENSE.md).

## Credits

Designed and built by **Andy Hudson-Smith**, [digitalurban.org](https://www.digitalurban.org).
