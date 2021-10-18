# WindSpeedGauge

Arduino Code for the 3D Printed Wind Speed Gauge

 In addition to reading the MQTT data and using the Servo Easing Library for the servo, the code also includes a time function, allowing the gauge to turn the LED lights/Servo on and off at set times. This is used to turn off at night.
 
![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/IMG_0292.jpg)

The code can be used to create any gauge with a 360 degree range.

![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/IMG_0031.JPG)

See comments in the .ino file for set up and calibration details - in the top image we are using a 270 degree range and an MQTT feed of wind speed on one guage and Air Quality (from a Davis Air Quality unit) on the other.

Wiring as per below on a NodeMCU, also commented in the Arduino code - pins can be changed according to your own board.

 ![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/GaugewithLEDS.png)
 
 
 ![Screen](https://github.com/ucl-casa-ce/WindSpeedGauge/blob/main/gaugeparts.png)
 
Note - The ServoEasing Library requires version 2.3.4 to work
