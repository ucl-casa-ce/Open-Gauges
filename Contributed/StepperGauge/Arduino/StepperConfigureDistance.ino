#include <AccelStepper.h>

// Define motor control pins for 28BYJ-48
#define motorPin1  9
#define motorPin2  10
#define motorPin3  11
#define motorPin4  12

// Define limit switch pin
#define limitSwitchPin 8

// Define stepper parameters
#define stepsPerRevolution 2048    // Steps per revolution of the output shaft
const float gearCircumference = 1; // Circumference of the gear in cm

// Calculate steps per cm
const float stepsPerCm = stepsPerRevolution / gearCircumference;

// Create AccelStepper instance
AccelStepper stepper(AccelStepper::HALF4WIRE, motorPin1, motorPin3, motorPin2, motorPin4);

// Global variable to track the target position in steps
long targetPositionSteps = 0;

void homeStepper() {
  Serial.println("Homing stepper motor...");

  // Temporary slow settings just for safe homing
  stepper.setMaxSpeed(300);    
  stepper.setAcceleration(100); 
  stepper.setSpeed(-100);      // Move backward toward switch

  // Run backward until the limit switch is pressed (LOW or HIGH depending on your switch wiring)
  while (digitalRead(limitSwitchPin) == HIGH) { 
    stepper.runSpeed();
  }

  stepper.stop(); // Stop the motor
  stepper.setCurrentPosition(0); // Set this physical spot as 0
  Serial.println("Homing complete. Stepper position reset to 0.");
}

void moveToAbsolutePosition() {
  Serial.println("\nEnter the absolute position to move to (in cm):");

  while (Serial.available() == 0) {
    // Wait for user input in the Serial Monitor
  }

  String input = Serial.readStringUntil('\n');
  float targetPositionCm = input.toFloat();

  // Convert the target position from cm to steps
  targetPositionSteps = targetPositionCm * stepsPerCm;

  Serial.print("Moving to absolute position (cm): ");
  Serial.println(targetPositionCm);
  Serial.print("Target position in steps: ");
  Serial.println(targetPositionSteps);

  // Move the stepper to the target position
  stepper.moveTo(targetPositionSteps);

  // Run the motor with smooth ease-in/out until it arrives
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }

  Serial.println("Move complete.");
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give Serial Monitor time to open

  Serial.println("Initializing system...");

  // Configure limit switch pin
  pinMode(limitSwitchPin, INPUT_PULLUP);

  // 1. Run the homing sequence first
  homeStepper();

  // 2. Now set your global speeds for the normal user-controlled moves
  stepper.setMaxSpeed(800);      // Maximum speed
  stepper.setAcceleration(300);  // Smooth ease-in/out setting

  Serial.println("System ready for user input.");
}

void loop() {
  // This will prompt you, wait for a number, execute the smooth move, and repeat
  moveToAbsolutePosition();
}
