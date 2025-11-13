# Open Gauges
### Arduino Code, 3D printer files and Graphics Templates for the Open Gauges project
 ![Fusion Dial](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/imgs/5DialsFrontsm.png)
 
The  Open Gauges Project project comes from the [Connected Environments Lab](https://connected-environments.org/) at [The Centre for Advanced Spatial Analysis](https://www.ucl.ac.uk/bartlett/casa), [University College London](https://www.ucl.ac.uk), it was initiated as part of the Module on Sensor Data Visualisation, part of the MSc in Connected Environments.


The project aims to allow open source data gauges to be built, modified, and viewed as both physical (3d printed) and digital gauges. Depending on the user’s preference the models can be made to run from any online data source with a data feed - from Weather Data with Air Pressure, Temperature, Wind Speed etc though to Air Quality Gauges, Noise Meters, Energy etc. At the current time files to create the physical gauges are provided with versions to work in Augmented Reality, via Unity, incoming. The project was created by [Professor Andrew Hudson-Smith](https://connected-environments.org/people/) and [Dr Valerio Signorelli](https://connected-environments.org/people/). 

![parts](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/imgs/parts.png)

<img src="https://github.com/ucl-casa-ce/Open-Gauges/blob/main/Contributed/Linear%20Stepper%20Gauge/WindStepperLinear.png" align="right" width="25%">

It is requested that new Guages created - either via additions to the Arduino Code or via new Graphics file are added as new branches, creating a reposit
ory of both physical and digital gauges. Notable new additions will be featured in this main thread.

A total of 5 Dial Graphics were provided in the initial release - sized to fit into the 3D printed cases. The collection now (13th November 2025) includes an additional [Noise Gauge](https://github.com/ucl-casa-ce/Open-Gauges/tree/main/Contributed/NoiseGauge), [Neopixel Barometer](https://connected-environments.org/open-gauges/neopixel-barometer/) a [Voltmeter Gauge](https://connected-environments.org/making/open-gauges-the-voltmeter-gauge/), [Light Saber Data Tube](https://github.com/ucl-casa-ce/Open-Gauges/tree/main/Contributed/Lightsaber_DataTube), [Ships Lamp Gauge](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/Contributed/ShipsLamp/Readme.md), a [Dial Based Stepper Motor Data Gauage](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/Contributed/StepperGauge/) and a [Linear Stepper Motor Data Gauge](https://github.com/ucl-casa-ce/Open-Gauges/tree/main/Contributed/Linear%20Stepper%20Gauge).



<img src="https://github.com/ucl-casa-ce/Open-Gauges/blob/main/Graphics%20Files/initialdials.png" align="centre" width="70%">


The 5 Dial Graphics are - Temperature (-10 to 40 oC), Wind Speed (0-60 mph), Wind Dir (0 - 360), Air Pressure (950 - 1050 mb) and Co2 (400 - 1400 ppm).

In addition to reading the MQTT data and using the Servo Easing Library for the servo, the code also includes a time function, allowing the gauge to turn the LED lights/Servo on and off at set times. This is used to turn off at night and on again in the morning.
 
The code can be used to create any gauge with a range from 180 to 360 degrees using a standard SG90 servo. A gear train is used to extend the servo range with the ability to calibrate in the code. On load, the servo performs a sweep function, to aid the calibration process.

<img src="https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/imgs/IMG_0292.jpg" align="centre" width="70%">

See comments in the .ino file for set up and calibration details - in the above image we are using a 270 degree range and an MQTT feed of wind speed on one guage and Air Quality (from a Davis Air Quality unit) on the other.

<img src="https://github.com/ucl-casa-ce/Open-Gauges/blob/main/imgs/techdraw3.png" align="centre" width="70%">

<img src="https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/imgs/IMG_0031.JPG" align="centre" width="70%">

The gauges are made to be as simple as possible to make but allow enough flexibilty to allow them to be used to display a wide range of data types.

