# NeoPixel Lightsaber Data Tube - Wind Meter

![Lightsaber Data Tube](https://connected-environments.org/wp-content/uploads/2023/10/LightSaberTitle-1024x497.jpg)

Harness the power of the Force to visualize real-time data with a NeoPixel Lightsaber! This project uses a lightsaber tube filled with LEDs to create a dynamic display of wind speeds. It's part of the Open Gauges Project, providing open-source code and 3D print files for various data gauges.

## Why a Lightsaber?

A lightsaber offers an excellent medium for light diffusion, making it perfect for a uniform and visually appealing data display. A full guide to the build can be found in the accompanying [blog post] () over at Connected Environments/Digital Urban.

![Fusion360](https://connected-environments.org/wp-content/uploads/2023/11/WindNeoPixelLightSaberTube_2023-Nov-08_03-20-52PM-000_CustomizedView14012120140.png)

## Hardware Components

- 1" OD Thin-Walled Trans White Polycarbonate Blade Tube
- One-meter-long foam diffuser tube
- Blade Diffusion Film
- 144 WS2812b NeoPixel strip
- Pi PicoW, specifically the Plasma Stick 2040W PicoW Aboard by Pimoroni
- 3D printed top and bottom end mounts (Files available in the 3D Files Folder)
- 3D printed markers - 5 to 55mph and beaufort wind scale text


## NeoPixels

NeoPixels are individually addressable RGB LEDs, perfect for representing data like wind speeds with varying colors.

## Representing Wind Speed

The code maps wind speeds to colors on the LED strip:

- 0 to 10: BLUE (Low winds)
- 10 to 20: GREEN (Fresh winds)
- 20 to 30: YELLOW (Moderate winds)
- 30 to 40: ORANGE (Strong winds)
- Above 40: RED (Gale Force and above)

## Overview

The project uses the NeoPixel library  and MQTT protocol for real-time data streaming.


Contributions
Contributions are welcome! Please fork the repository and submit a pull request with your improvements.

License
This project is licensed under the MIT License - see the LICENSE.md file for details.

Acknowledgements
[The Saber Armory](https://thesaberarmory.com/collections/neopixels-led-strips) for hardware components
[Pimoroni for the Plasma Stick 2040W PicoW Aboard](https://shop.pimoroni.com/products/plasma-stick-2040-w?variant=40359072301139) 
[Neopixel Micropython Library](https://github.com/blaz-r/pi_pico_neopixel?ref=bhave.sh)
[MQTT_as Library](https://github.com/peterhinch/micropython-mqtt) 
All contributors to the Open Gauges Project
