# NeoPixel Barometer (Weather Flow Version)

The Barometer has been largey unchanged since.....

Incoming.. last edit 28th October 2021

<img align="right" src="https://i0.wp.com/connected-environments.org/wp-content/uploads/2021/10/neopixelbarolongright.png?">

## Data Source

The barometer uses the Better Forecast API from [Weather Flow](https://weatherflow.github.io/Tempest/).

## Data displayed

The Neopixel Barometer displays current sea level air pressure (Mb), conditions - Sunny, Partly Cloudy, Cloudy, Rain, Snow, Thunderstorm, Foggy and the current pressure trend  - Rising, Steady, Falling.

The data updates every minute.

## 3D printed model



## Wood

The Neopixel strip is mounted onto a thin strip of wood approx 1.25 meters long using the fixings that come with the Neopixel Strip. The Text/Numbers are 3D printed (as above) and glued on the back of the wood. The use of wood/mounting is to allow flexibilty - ie mount it however you like. 

### Hardware

The hardware has been selected to be as low cost as possible - 

- A Raspberry Pi  - We used the Raspberry Pi Zero W
- 1 Meter 144 Addressable Neopixel Strip (NeoPixel/WS2812/SK6812 compatible) - [Example here from The PiHut](https://thepihut.com/products/flexible-rgb-led-strip-neopixel-ws2812-sk6812-compatible-144-led-meter)

### Code and library

The full code is provided in the tempestbarometer.py file, it requires an API and Station key, see the [Weather Flow API page for details] (https://weatherflow.github.io/Tempest/api/).

**Libraries and version used**

Libraries used - 

requests
json
time
neopixel
board

## Digital model

The model is provided in Fusion 360 for any edits to wording, sizing etc.
