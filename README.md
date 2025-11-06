# Open Gauges
### Arduino Code, 3D printer files and Graphics Templates for the Open Gauges project
 ![Fusion Dial](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/imgs/5DialsFrontsm.png)
 
Run out the [Connected Environments Lab](https://connected-environments.org/) at [The Centre for Advanced Spatial Analysis](https://www.ucl.ac.uk/bartlett/casa), [University College London](https://www.ucl.ac.uk), the Open Gauges Project was initiated as part of the Module on Sensor Data Visualisation, part of the MSc in Connected Environments.

The project aims to allow open source data gauges to be built, modified, and viewed as both physical (3d printed) and digital gauges. Depending on the user’s preference the models can be made to run from any online data source with a data feed - from Weather Data with Air Pressure, Temperature, Wind Speed etc though to Air Quality Gauges, Noise Meters, Energy etc. At the current time files to create the physical gauges are provided with versions to work in Augmented Reality, via Unity, incoming. The project was created by [Professor Andrew Hudson-Smith](https://connected-environments.org/people/) and [Dr Valerio Signorelli](https://connected-environments.org/people/). 

![parts](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/imgs/parts.png)

It is requested that new Guages created - either via additions to the Arduino Code or via new Graphics file are added as new branches, creating a repository of both physical and digital gauges. Notable new additions will be featured in this main thread.

A total of 5 Dial Graphics were provided in the initial release - sized to fit into the 3D printed cases. The collection now (6th November 2025) includes an additional [Noise Gauge](https://github.com/ucl-casa-ce/Open-Gauges/tree/main/Contributed/NoiseGauge), [Neopixel Barometer](https://connected-environments.org/open-gauges/neopixel-barometer/) and a [Voltmeter Gauge](https://connected-environments.org/making/open-gauges-the-voltmeter-gauge/, [Light Saber Data Tube] (https://github.com/ucl-casa-ce/Open-Gauges/tree/main/Contributed/Lightsaber_DataTube) and the most recent [Stepper Motor Data Gauage] (https://github.com/ucl-casa-ce/Open-Gauges/blob/main/Contributed/StepperGauge/). 

<p align="center">
<img src="https://github.com/ucl-casa-ce/Open-Gauges/blob/main/Graphics%20Files/initialdials.png">

</p>
The 5 Dial Graphics are - Temperature (-10 to 40 oC), Wind Speed (0-60 mph), Wind Dir (0 - 360), Air Pressure (950 - 1050 mb) and Co2 (400 - 1400 ppm).

In addition to reading the MQTT data and using the Servo Easing Library for the servo, the code also includes a time function, allowing the gauge to turn the LED lights/Servo on and off at set times. This is used to turn off at night and on again in the morning.
 
The code can be used to create any gauge with a range from 180 to 360 degrees using a standard SG90 servo. A gear train is used to extend the servo range with the ability to calibrate in the code. On load, the servo performs a sweep function, to aid the calibration process.

![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/imgs/IMG_0292.jpg)

See comments in the .ino file for set up and calibration details - in the above image we are using a 270 degree range and an MQTT feed of wind speed on one guage and Air Quality (from a Davis Air Quality unit) on the other.

![techdrwaing](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/imgs/techdraw3.png)

![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/imgs/IMG_0031.JPG)

The gauges are made to be as simple as possible to make but allow enough flexibilty to allow them to be used to display a wide range of data types.

## Parts List

The main parts are:

* _Node MCU Arduino Board_ - we have been using the ([MakerHawk boards](https://www.amazon.co.uk/MakerHawk-Internet-Development-Wireless-Micropython/dp/B07M8Q38LK/ref=sr_1_4?dchild=1&keywords=nodemcu&qid=1634650644&sr=8-4)). However, any Arduino compatiable board will suffice, the ease of using the above boards is the code will work without and changes to the pins.

* _SG90 Servo_ - any SG90 style servo will work, we would however recommend the MG90S Micro Servo as it provides a smoother travel to the gauage pointer.

* _Lights_ - [Pimorini White LED Backlight Module – 38.7mm x 11.9mm x 2mm](https://shop.pimoroni.com/products/white-led-backlight-module?variant=36999548170), although any low power led will also suffice

* _PLA for 3D Printing_ - Any PLA for the main parts, the dials graphics are printed on paper and laid flat on a disc (see 3D Printer Files) printed in transparent PLA. This can be left out but it allows the dial to lay flat and provides a nice diffused light. eSun Transparent PLA works well.

## Wiring 

As per below on a NodeMCU, also commented in the Arduino code - pins can be changed according to your own board.

 ![Screen](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/imgs/servoneopixelnode.jpg)
 
Note - The ServoEasing Library requires version 2.3.4

Newly added (Oct 2022) Micropython version uses a Raspberry Pi Pico W with a Neopixel strip and SG90 servo:

![parts](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/imgs/micropygauge2022.png)

# Augmented Reality Open Gauges

Note this section is a work in progress, a Unity project file is incoming.

 ![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/imgs/blenderstart.png)

The Fusion 360 file can be exported into a variety of formats for import into systems such as Sketchfab, Blender and Unity - via the Unity engine it can be developed into an Augmented Reality version.

<p align="center">
<img src="https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/imgs/ARdial.png">

</p>

See the [twitter feed](https://twitter.com/digitalurban/status/1429775146538184704) for the full clip or to view and interact with the 3D model, it is available on [Sketchfab](https://skfb.ly/ooRqt).


 ![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/imgs/gaugeparts.png)
