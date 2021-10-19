# Open Gauges

 ![Fusion Dial](https://github.com/ucl-casa-ce/Open-Gauges/blob/main/fusionstart.png)
 
Arduino Code, 3D printer files and Illustrator Templates for the Open Gauges project. Run out the [Connected Environments Lab](https://connected-environments.org/) at [The Centre for Advanced Spatial Analysis](https://www.ucl.ac.uk/bartlett/casa), [University College London](https://www.ucl.ac.uk), the Open Gauges Project is produced as part of the Module on Sensor Data Visualisation, part of the MSc in Connected Environments.

The project aims to allow data gauges to be built, modified, and viewed as both physical and digital gauges. Depending on the user’s preference the models can be made to run from any online data source with an MQTT feed - from Weather Data with Air Pressure, Temperature, Wind Speed etc though to Air Quality Gauges, Noise Meters, Energy etc. At the current time files to create the physical gauges are provided with versions to work in Augmented Reality, via Unity, incoming. The project was created by [Professor Andrew Hudson-Smith](https://connected-environments.org/people/) and [Dr Valerio Signorelli](https://connected-environments.org/people/). 

It is requested that new Guages created - either via additions to the Arduino Code or via new Graphics file are added as new branches, creating a repository of both physical and digital gauges. New additions will be featured in this main thread.

In addition to reading the MQTT data and using the Servo Easing Library for the servo, the code also includes a time function, allowing the gauge to turn the LED lights/Servo on and off at set times. This is used to turn off at night and on again in the morning.

![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/IMG_0031.JPG)
 
The code can be used to create any gauge with a range from 180 to 360 degrees using a standard MG90 servo. A gear train is used to extend the servo range with the ability to calibrate in the code. On load, the servo performs a sweep function, to aid the calibration process.

![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/IMG_0292.jpg)

See comments in the .ino file for set up and calibration details - in the above image we are using a 270 degree range and an MQTT feed of wind speed on one guage and Air Quality (from a Davis Air Quality unit) on the other.

Wiring as per below on a NodeMCU, also commented in the Arduino code - pins can be changed according to your own board.

 ![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/GaugewithLEDS.png)
 
 ![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/gaugeparts.png)
 
Note - The ServoEasing Library requires version 2.3.4
