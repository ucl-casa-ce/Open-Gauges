# Open Gauges

 ![Screen]https://github.com/ucl-casa-ce/Open-Gauges/blob/main/fusionstart.png
 
Arduino Code, 3D printer files and Illustrator Templates for the Open Gauges project. Run out the Connected Environments Lab at The Centre for Advanced Spatial Analysis, University College London, the Open Gauges Project is produced as part of the Module on Sensor Data Visualisation, part of the MSc in Connected Environments. The project aims to allow data gauges to be built, modified, and viewed as both physical and digital gauges. Depending on the user’s preference the models can be made to run from any online data source with an MQTT feed - from Weather Data with Air Pressure, Temperature, Wind Speed etc though to Air Quality Gauges, Noise Meters, Energy etc. At the current time files to create the physical gauges are provided with versions to work in Augmented Reality, via Unity, incoming. The project was created by Professor Andrew Hudson-Smith and Dr Valerio Signorelli. 

It is requested that new Guages created - either via additions to the Arduino Code or via new Graphics file are added as new branches, creating a repository of both physical and digital gauges. New additions will be featured in this main thread.


![Screen](https://user-images.githubusercontent.com/50172263/137885191-930a2848-2113-4476-9749-7d5de1d089eb.png)


 In addition to reading the MQTT data and using the Servo Easing Library for the servo, the code also includes a time function, allowing the gauge to turn the LED lights/Servo on and off at set times. This is used to turn off at night.
 
 ![Screen]https://github.com/ucl-casa-ce/Open-Gauges/blob/main/fusionstart.png
 
![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/IMG_0292.jpg)

The code can be used to create any gauge with a 360 degree range.

![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/IMG_0292.jpg)

![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/IMG_0031.JPG)

See comments in the .ino file for set up and calibration details - in the top image we are using a 270 degree range and an MQTT feed of wind speed on one guage and Air Quality (from a Davis Air Quality unit) on the other.

Wiring as per below on a NodeMCU, also commented in the Arduino code - pins can be changed according to your own board.

 ![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/GaugewithLEDS.png)
 
 
 ![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/gaugeparts.png)
 
Note - The ServoEasing Library requires version 2.3.4
