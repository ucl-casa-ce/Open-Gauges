# Contributed Project: Stepper Gauge

This folder contains a contributed version of the Open Gauge that uses a **stepper motor** for precise needle movement, a **limit switch** for automatic calibration, and a **NeoPixel** for advanced lighting.

This project is an alternative to the main servo-based gauge in the root of the `Open-Gauges` repository.

## Overview

This design uses a stepper motor (like the 28BYJ-48) which offers high-precision, 360-degree movement without the jitter or limited range of a standard servo. The limit switch allows the gauge to "home" itself on startup, ensuring the needle always starts at a known zero position.

!(https://raw.githubusercontent.com/ucl-casa-ce/Open-Gauges/main/Contributed/StepperGauge/StepperGauge.jpg)

To ensure accuracy, this project uses a **two-step calibration process**:
1.  First, you run the **`DefineDistance.ino`** sketch to find the exact number of steps your gauge's needle needs to travel from zero to its maximum position.
2.  Second, you update the main **`StepperGauge.ino`** code with this number.

## Hardware Components

* **Arduino-compatible Board:** Any board like an Arduino Uno, Nano, or a NodeMCU.
* **Stepper Motor:** 28BYJ-48 5V stepper motor.
* **Stepper Driver:** ULN2003 driver board (which often comes with the 28BYJ-48).
* **Limit Switch:** A small microswitch to detect the needle's zero position.
* **NeoPixel:** A single WS2812B LED (or a small strip) for illumination.
* **Power Supply:** 5V power supply.
* **Connecting Wires**

## Software & Code

This project includes two Arduino sketches:

1.  **`DefineDistance.ino`**: You run this sketch **first**. It helps you find the total number of steps from the "home" position (at the limit switch) to the maximum position you want your gauge to travel.
2.  **`StepperGauge.ino`**: This is the **main operational code** for the gauge. It requires the calibration value from the first sketch to work correctly.

### Required Libraries

You must install the following libraries in your Arduino IDE for both sketches:

* **`AccelStepper`**: For advanced stepper motor control.
* **`Adafruit_NeoPixel`**: To control the NeoPixel LED.

## Wiring

You must wire the components correctly for the calibration and main sketches to work.

* **Stepper Driver:**
    * `IN1`, `IN2`, `IN3`, `IN4` on the ULN2003 board connect to four digital pins on the Arduino (check the code for exact pins).
    * `+` and `-` on the driver board connect to your 5V power supply and Ground.
* **Limit Switch:**
    * One pin connects to an Arduino digital pin (e.g., D5).
    * The other pin connects to Ground (GND). The code will use the Arduino's internal `INPUT_PULLUP` resistor.
* **NeoPixel:**
    * `5V` connects to the 5V power supply.
    * `GND` connects to Ground.
    * `Data In (DI)` connects to a single Arduino digital pin (e.g., D6).

![Wiring diagram for the Stepper Gauge on a breadboard](https://raw.githubusercontent.com/ucl-casa-ce/Open-Gauges/main/Contributed/StepperGauge/StepperGauge_bb.jpg)

## How to Use: Calibration & Setup

Follow these steps carefully to get your gauge running.

### Step 1: Assemble Hardware
Assemble the 3D-printed gauge and wire all the components as described in the **Wiring** section. Ensure the limit switch is placed so the needle will press it at the desired "zero" position.

### Step 2: Run Calibration Sketch
This is the most important step for accuracy.

1.  Open the **`DefineDistance.ino`** sketch in your Arduino IDE.
2.  Verify the pin definitions at the top of the code match your wiring.
3.  Upload the sketch to your Arduino board.
4.  Open the **Serial Monitor** (set to 9600 baud).
5.  The sketch will first "home" the gauge by moving the needle until it hits the limit switch.
6.  It will then likely ask you to send commands via the Serial Monitor (e.g., 'f' for forward, 'b' for back) to manually move the needle.
7.  Move the needle to the **maximum position** you want for your gauge dial.
8.  The sketch will report the total number of steps from home to this max position. **Write this number down.**

### Step 3: Configure Main Sketch
Now you will update the main gauge code with your calibration value.

1.  Open the **`StepperGauge.ino`** sketch in the Arduino IDE.
2.  Near the top of the code, find a variable named something like `MAX_STEPS` or `fullScaleSteps`.
3.  Change the value of this variable to the **number you wrote down** from Step 2.
4.  Configure the rest of the sketch as needed (e.g., WiFi/MQTT credentials).

### Step 4: Upload and Run
1.  Upload the modified `StepperGauge.ino` to your Arduino.
2.  Power the project. The gauge will automatically home itself and is now calibrated and ready to display data.