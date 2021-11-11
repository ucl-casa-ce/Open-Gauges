# The Voltmeter Gauge

The voltmeter gauge has a 5V range and runs from an Arduino Uno Wifi rev 2 as it outputs a full 5 volts from its Pulse Width Modulation pins. 

Most Arduino boards only output 3V, in which case the code can be simply edited to use a 3V Voltmeter. The voltmeter has a loading sweep on power-up and then connects to wifi and an MQTT data feed, in our case a feed from our weather station with wind speed updated every 3 seconds.

[!Voltmeter Gauge](https://connected-environments.org/wp-content/uploads/2021/11/front-scaled.jpg) 

Included in the folders are the Arudino code, STL file to print, Fusion 360 to edit/adapt the enclosure and an illustator template to edit to your own data/values.

See [the Voltmeter Gauge Page over at Connected Environments](https://connected-environments.org/?p=6286&preview=true) for further details.



Note - Run out the [Connected Environments Lab](https://connected-environments.org/) at [The Centre for Advanced Spatial Analysis](https://www.ucl.ac.uk/bartlett/casa), [University College London](https://www.ucl.ac.uk), the Open Gauges Project was initiated as part of the Module on Sensor Data Visualisation, part of the MSc in Connected Environments.

The project aims to allow open source data gauges to be built, modified, and viewed as both physical (3d printed) and digital gauges. Depending on the user’s preference the models can be made to run from any online data source with a data feed - from Weather Data with Air Pressure, Temperature, Wind Speed etc though to Air Quality Gauges, Noise Meters, Energy etc. At the current time files to create the physical gauges are provided with versions to work in Augmented Reality, via Unity, incoming. The project was created by [Professor Andrew Hudson-Smith](https://connected-environments.org/people/) and [Dr Valerio Signorelli](https://connected-environments.org/people/). 
