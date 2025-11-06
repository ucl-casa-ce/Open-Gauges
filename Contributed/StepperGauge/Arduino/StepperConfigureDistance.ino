#include <AccelStepper.h>

// Define motor control pins for 28BYJ-48
#define motorPin1  9
#define motorPin2  10
#define motorPin3  11
#define motorPin4  12

// Define stepper parameters
#define stepsPerRevolution 2048    // Steps per revolution of the output shaft
const float gearCircumference = 1; // Circumference of the gear in cm

// Calculate steps per cm
const float stepsPerCm = stepsPerRevolution / gearCircumference;

// Create AccelStepper instance
AccelStepper stepper(AccelStepper::HALF4WIRE, motorPin1, motorPin3, motorPin2, motorPin4);

// Global variable to track the target position in steps
long targetPositionSteps = 0;

void moveToAbsolutePosition() {
  Serial.println("Enter the absolute position to move to (in cm):");

  while (Serial.available() == 0) {
    // Wait for user input
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

  // Run the motor until the target position is reached
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }

  Serial.println("Move complete.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Initializing system...");

  // Configure stepper motor
  stepper.setMaxSpeed(400);    // Max speed in steps per second
  stepper.setAcceleration(400); // Acceleration in steps per second^2

  Serial.println("System ready.");
}

void loop() {
  moveToAbsolutePosition();
}