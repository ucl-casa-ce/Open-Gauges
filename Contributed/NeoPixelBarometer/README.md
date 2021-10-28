# NeoPixel Barometer (Weather Flow Version)

The Open Guages project aims to allow open-source data gauges to be built, modified, and viewed as both physical (3d printed) and digital gauges. Depending on the user’s preference the models can be made to run from any online data source with a data feed - from Weather Data with Air Pressure, Temperature, Wind Speed etc though to Air Quality Gauges, Noise Meters, Energy etc.

Part of the initial release, from the Connected Environments Team at The Bartlett Centre for Advanced Spatial Analysis, University College London, and alongside the more traditional 'dial style' gauges, is our Neopixel Barometer.

Designed to be as simple as possible it is powered by a Raspberry Pi and uses the data feed from the Weather Flow API, making it open to any users with a Weather Flow Tempest Weather Station. As with all Open Gauges, this feed can be changed for any other Weather data that includes Air Pressure, Conditions and Pressure Trend.

<img align="right" src="https://i0.wp.com/connected-environments.org/wp-content/uploads/2021/10/neopixelbarolongright.png?">

## Data Source

The barometer uses the Better Forecast API from [Weather Flow](https://weatherflow.github.io/Tempest/), provided as JSON.

## Data displayed

The Neopixel Barometer displays current sea level air pressure (Mb), conditions - Sunny, Partly Cloudy, Cloudy, Rain, Snow, Thunderstorm, Foggy and the current pressure trend  - Rising, Steady, Falling.

The data updates every minute.

## 3D printed model

The main barometer markers - ie STORM, FAIR, CHANGE, as well as the numbers - 950, 960 etc are provided as seperate .stl files to 3D print. This allow easy alignment with the Neopixel strip with the correct pixel.

<img align="left" src="https://connected-environments.org/wp-content/uploads/2021/10/neopixel3dprint.png">

The conditions come in a left and right sections, again to be alighed once the Neopixel strip is mounted. The Trend titles are a single 3D print.

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

- requests
- json
- time
- neopixel
- board

## Digital model

The model is provided in Fusion 360 for any edits to wording, sizing etc.
