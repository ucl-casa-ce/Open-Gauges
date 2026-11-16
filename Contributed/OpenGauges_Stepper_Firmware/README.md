# Open Gauges: one firmware for every stepper gauge (beta)

*Part of the **Open Gauges** project: open, 3D-printable dials for live environmental data.*

One sketch runs all five stepper gauges: [wind direction](../OpenGauges_WindGauge_Stepper/), [wind speed](../OpenGauges_WindSpeedGauge_Stepper/), [air pressure](../OpenGauges_PressureGauge_Stepper/), [temperature](../OpenGauges_TemperatureGauge_Stepper/) and [daily rainfall](../OpenGauges_RainGauge_Stepper/). You choose which gauge it is from your phone, and set everything else there too. **There's nothing to edit in the code.**

> **Beta.** The per-gauge sketches in each gauge's `firmware/` folder still work and stay in place until this one has been tested on every gauge.

- **Set up from your phone:** Wi-Fi, gauge type, weather feed and location, on a setup page the gauge serves itself. No Wi-Fi passwords in the sketch.
- **Settings page while it runs:** backlight (on at dusk, always on or always off), weather feed with a live check that data is arriving, pointer tuning you can watch as you adjust, an LED finder, and gauge type.
- **Wireless updates (OTA):** after the first USB upload, upload over Wi-Fi from the Arduino IDE.
- **Never hangs on homing:** if the magnet isn't found the gauge carries on, shows a red warning LED and retries every hour.
- **Works straight away:** the feed is filled in with a live demo station, so a new gauge moves as soon as it's online. Swap in your own feed when you're ready.

## Hardware

Exactly the same as the individual gauges: ESP32-C3 Super Mini, 28BYJ-48 stepper on a ULN2003 driver, KY-003 hall sensor with a 5 mm magnet, and a WS2812B strip. See the [wiring diagram](../OpenGauges_WindGauge_Stepper/docs/Wiring_ESP32C3.png).

| ESP32-C3 Super Mini | Connects to |
|---|---|
| GPIO4 / GPIO3 / GPIO1 / GPIO0 | ULN2003 IN1 / IN2 / IN3 / IN4 |
| GPIO7 | KY-003 S |
| GPIO5 (optional) | KY-003 + (powers the sensor only while homing; or use 5V) |
| GPIO10 | WS2812B DIN |
| 5V | ULN2003 +, KY-003 +, LED 5V |
| GND | all grounds |
| BOOT button (GPIO9, on the board) | opens the setup and settings pages |

## Uploading (first time, by USB)

1. **Libraries** (Library Manager): AccelStepper, PubSubClient, ArduinoJson (v7), Adafruit NeoPixel, and **WiFiManager by tzapu** (2.0.17 or later). Several libraries have similar names: it must be the one by tzapu.
2. **Tools menu:** Board "ESP32C3 Dev Module", USB CDC On Boot "Enabled".
3. **Partition Scheme:** **"Minimal SPIFFS (1.9MB APP with OTA)"**. The sketch also fits the default scheme, but only just, so use Minimal SPIFFS to leave room for updates. Don't use "Huge APP": it has no room for wireless updates.
4. Open [`firmware/OpenGauges_ESP32C3/OpenGauges_ESP32C3.ino`](firmware/OpenGauges_ESP32C3/OpenGauges_ESP32C3.ino) and upload.

## First start: the setup page

The ring turns **cyan** and the gauge makes its own Wi-Fi network, **`OpenGauge-xxxx`** (password `opengauge`). Join it on your phone and the setup page opens; if it doesn't, browse to `192.168.4.1`. Tap **Configure WiFi**, then:

1. Pick your Wi-Fi network and type its password.
2. **Which gauge is this?** Choosing the type fills in that gauge's feed topic and units.
3. **Weather feed:** MQTT server, port, optional username and password, topic, and the units of a key with no units in its name. Leave the demo values in to start with if you like.
4. **Location:** latitude and longitude, for switching the backlight on at dusk.
5. **Save.** The gauge joins your Wi-Fi, homes, runs its self-test and starts showing live data.

The gauge won't run until a type is chosen, so a gauge with no type always opens the setup page.

## The settings page

With your phone on the same Wi-Fi, browse to **`http://opengauge-<type>-xxxx.local`**, for example `http://opengauge-rain-a3f0.local`. If `.local` addresses don't work on your phone (some Android phones), use the gauge's IP address, which the Serial Monitor shows at start-up, or find it in your router's device list.

| Section | What it does |
|---|---|
| Backlight | on at dusk (until an off time), always on or always off; location; brightness. Applies and saves straight away, and shows today's dusk time. |
| Weather feed | server, port, username, password, topic and units. Applies and saves straight away. The status line shows whether data is arriving: *not connected*, *connected but no messages on this topic*, *messages arriving but no reading found in them*, or *✓ data arriving*. |
| Pointer tuning | see below |
| LED finder | lights one LED red for 5 seconds, to find the numbers beside the words on your dial |
| Gauge type | changes the type and restarts. Each type keeps its own feed and tuning. |

The page is served for **10 minutes** after start-up, or after you last used it, so it isn't running all the time. To open it again, restart the gauge or press its **BOOT** button: the ring flashes cyan.

### Pointer tuning

1. **Home offset:** how far the pointer turns on from the magnet to reach the top of the dial (12 o'clock). Type a value and press **Apply & home**: the gauge finds the magnet and turns on by that amount. If it stops left of the top, increase the offset. If it goes past, decrease it.
2. **Backlash:** the gearbox has a little play. **Apply & test** brings the pointer to the top from the left, pauses, then brings it from the right. If the second stop is to the right of the first, increase the backlash until they match. Too much does no harm.
3. **Ends of the scale** (not on the wind direction gauge): if the top is right but one end sits a degree or two off its mark, trim that end. The correction is full at that end and fades to nothing at the top.
4. The dial-check buttons and **Sweep** move the pointer to the marks, so you can check the whole scale.
5. Press **Save tuning**.

While you're tuning, the gauge ignores the feed. It goes back to live readings by itself after 10 minutes, or when you press **Back to live readings**.

## Wireless updates

Once the gauge is on your Wi-Fi it shows up in the Arduino IDE under **Tools → Port** as a network port, `opengauge-<type>-xxxx`. Choose it and upload as normal; the ring fills green as the update goes in. The IDE asks for a password even if you didn't set one: type anything. If you set a password for wireless uploads on the setup page, use that.

An update goes into a spare slot in the flash and only takes over once it's complete and checked, so an interrupted upload leaves the old firmware running. Your settings aren't touched.

## Changing settings that are only on the setup page

To change the Wi-Fi network or the wireless-upload password, hold **BOOT** while the ring is **red** at power-up. The setup page opens with your current settings, including the saved Wi-Fi password.

If the saved Wi-Fi can't be found at start-up, the setup page opens by itself for 3 minutes. If no one uses it, the gauge goes back to trying the saved network and never reboots in a loop.

## What each gauge does

| Gauge | Scale | Feed (JSON keys tried, best first) | Extra |
|---|---|---|---|
| Wind direction | full 360°, shortest way round | `windDir`, `wind_dir`, or a plain number | ignores calm (no direction) |
| Wind speed | 0–40 mph, 20 at the top | `windgust1min`, `windGust_mph/_kph/_mps/_knot`, `windGust`, or a plain number | red LED beside today's highest gust, reset at midnight |
| Air pressure | 950–1050 hPa, 1000 at the top | `barometer_mbar/_hPa/_inHg`, `barometer`, `pressure…` (inHg is spotted automatically) | 3-hour trend lights RISING (green) or FALLING (amber); pulses for a rapid change |
| Temperature | −10 to 40 °C, 15 at the top | `outTemp_C`, `outTemp_F`, `outTemp`, `temperature…` | 3-hour trend lights RISING (amber) or FALLING (blue) |
| Daily rainfall | 0–50 mm, stretched at the low end, 17 at the top | `dayRain_mm/_cm/_in`, `dayRain` | the station resets it at midnight |

A topic carrying just a number also works. Its units come from how the topic name ends, e.g. `…/dayRain_cm`, `…/outTemp_F`, `…/gust_kph`.

## Start-up colours

| Ring | Stage |
|---|---|
| Red | started (hold BOOT now for the setup page) |
| Cyan | setup page open |
| Blue | connecting to Wi-Fi |
| Yellow | homing and measuring one turn |
| Purple | self-test sweep |
| White (or dark) | ready |
| Green, filling | wireless update |

LED 0 (at the gap at the bottom) stays dim red if homing failed.

## Licence

Firmware: [MIT](firmware/LICENSE).

## Credits

Designed and built by **Andy Hudson-Smith**, [digitalurban.org](https://www.digitalurban.org).
