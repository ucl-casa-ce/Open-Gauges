# NeoPixel Lightsaber Data Tube - Wind Meter

![Lightsaber Data Tube](https://connected-environments.org/wp-content/uploads/2023/10/LightSaberTitle-1024x497.jpg)

Harness the power of the Force to visualize real-time data with a NeoPixel Lightsaber! This project uses a lightsaber tube filled with LEDs to create a dynamic display of wind speeds. It's part of the Open Gauges Project, providing open-source code and 3D print files for various data gauges.

## Why a Lightsaber?

A lightsaber offers an excellent medium for light diffusion, making it perfect for a uniform and visually appealing data display. A full guide to the build can be found in the accompanying [blog post] () over at Connected Environments/Digital Urban.

## Hardware Components

- 1" OD Thin-Walled Trans White Polycarbonate Blade Tube
- One-meter-long foam diffuser tube
- Blade Diffusion Film
- 144 WS2812b NeoPixel strip
- Pi PicoW, specifically the Plasma Stick 2040W PicoW Aboard by Pimoroni
- 3D printed top and bottom end mounts (Files available in the 3D Files Folder)


## NeoPixels

NeoPixels are individually addressable RGB LEDs, perfect for representing data like wind speeds with varying colors.

## Representing Wind Speed

The code maps wind speeds to colors on the LED strip:

- 0 to 10: BLUE (Low winds)
- 10 to 20: GREEN (Fresh winds)
- 20 to 30: YELLOW (Moderate winds)
- 30 to 40: ORANGE (Strong winds)
- Above 40: RED (Gale Force and above)

## Code Overview

The project uses the NeoPixel library and MQTT protocol for real-time data streaming. All necessary files are included in the [GitHub Repository](<link-to-github-repo>).

### Set Up NeoPixel Strip

```python
from neopixel import Neopixel
numpix = 144
pixels = Neopixel(numpix, 0, 15, "GRB")
pixels.brightness(255)

colors = {
    'BLUE': (0, 0, 255),
    'GREEN': (0, 255, 0),
    ...
}

### Wind Speed Ranges and Colors

WIND_SPEED_RANGE = [0, 60]
multiplier = numpix / (WIND_SPEED_RANGE[1] - WIND_SPEED_RANGE[0])

###Dynamic Updates and Tracking Peak Wind Speed
The script updates the display dynamically, showing the peak wind speed with a distinct RED pixel.

###MQTT and NeoPixel Lightsaber
The MQTT protocol streams data to the lightsaber, which then updates the display.

###Full Code
The full code is available in the Micropyton Folder.

Contributions
Contributions are welcome! Please fork the repository and submit a pull request with your improvements.

License
This project is licensed under the MIT License - see the LICENSE.md file for details.

Acknowledgements
The Saber Armory for hardware components
Pimoroni for the Plasma Stick 2040W PicoW Aboard

All contributors to the Open Gauges Project