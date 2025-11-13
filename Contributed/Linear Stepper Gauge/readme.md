# Linear Stepper Gauge

This folder contains a contributed version of the Open Gauge that uses a **stepper motor** for precise needle movement, and a **limit switch** for automatic calibration - the gauge uses a timer belt and a 3D printed pointer to indicate wind speed, it can be adapted to any MQTT data feed.

<img src="https://github.com/ucl-casa-ce/Open-Gauges/blob/main/Contributed/Linear%20Stepper%20Gauge/WindStepperLinear.png" align="right" width="25%">
</p> 

## Overview

This design uses a stepper motor (like the 28BYJ-48) which offers high-precision, 360-degree movement without the jitter or limited range of a standard servo. The limit switch allows the gauge to "home" itself on startup, ensuring the pointer always starts at a known zero position.

The main **`WindStepperTimerBeltwithLimitSwitch.ino`** has a distance calibration number, adjust for your range.

## Hardware Components

* **Arduino-compatible Board:** Any board like an Arduino Uno, Nano, or a NodeMCU.
* **Stepper Motor:** 28BYJ-48 5V stepper motor.
* **Stepper Driver:** ULN2003 driver board (which often comes with the 28BYJ-48).
* **Limit Switch:** A small microswitch to detect the pointers zero position.
* **Power Supply:** USB.
* **Timer Belt** [GT2 Timer Belt - ie](https://www.amazon.co.uk/Timing-Pulley-Tensioner-Torsion-Printer/dp/B0C54ZXM88/ref=sxin_15_pa_sp_search_thematic_sspa?content-id=amzn1.sym.0a6bbb1a-ed2d-4392-adfc-40ed1cfcd8e2%3Aamzn1.sym.0a6bbb1a-ed2d-4392-adfc-40ed1cfcd8e2&crid=L07UDXXKCZFX&cv_ct_cx=timing%2Bbelt%2Bgt2&keywords=timing%2Bbelt%2Bgt2&pd_rd_i=B0C54ZXM88&pd_rd_r=12141bbe-35d0-4c0a-8811-d7f399206de4&pd_rd_w=AVwDb&pd_rd_wg=JG5Mx&pf_rd_p=0a6bbb1a-ed2d-4392-adfc-40ed1cfcd8e2&pf_rd_r=H6T6R7VAGD99FGNPH875&qid=1763028887&sbo=RZvfv%2F%2FHxDF%2BO5021pAnSA%3D%3D&sprefix=timer%2Bbelt%2Bgt2%2Caps%2C99&sr=1-5-ad3222ed-9545-4dc8-8dd8-6b2cb5278509-spons&aref=vwr3X339Nm&sp_csd=d2lkZ2V0TmFtZT1zcF9zZWFyY2hfdGhlbWF0aWM&th=1)

<img src="https://github.com/ucl-casa-ce/Open-Gauges/blob/main/Contributed/Linear%20Stepper%20Gauge/timerbelt.jpg" align="centre" width="20%">

### Required Libraries

Requires the following libraries in your Arduino IDE for both sketches:

* **`AccelStepper`**: For advanced stepper motor control.
* 

## Wiring

You must wire the components correctly for the calibration and main sketches to work.

* **Stepper Driver:**
    * `IN1`, `IN2`, `IN3`, `IN4` on the ULN2003 board connect to four digital pins on the Arduino (check the code for exact pins).
    * `+` and `-` on the driver board connect to your Ground.
* **Limit Switch:**
    * One pin connects to an Arduino digital pin (e.g., D5).
    * The other pin connects to Ground (GND). The code will use the Arduino's internal `INPUT_PULLUP` resistor.
      
## 3D Printing

The stl folder contains the mount for the stepper motor, the pointer (which also joins together the timing belt) the limit switch and the end mount for the pulley. 
