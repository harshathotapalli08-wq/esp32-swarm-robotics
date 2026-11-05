
#include <Arduino.h>

// L298N motor driver pins
#define ENA 25
#define IN1 26
#define IN2 14

#define ENB 33
#define IN3 32
#define IN4 23

// Maximum PWM command: 0-255
const int MAX_SPEED = 255;

// Set true if the right motor's electrical
// direction must be reversed for forward travel.
const bool REVERSE_RIGHT_MOTOR = true;

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveForward() {
  // Left motor forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  // Right motor forward
  digitalWrite(IN3, REVERSE_RIGHT_MOTOR ? LOW : HIGH);
  digitalWrite(IN4, REVERSE_RIGHT_MOTOR ? HIGH : LOW);

  analogWrite(ENA, MAX_SPEED);
  analogWrite(ENB, MAX_SPEED);
}

void moveBackward() {
  // Left motor backward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  // Right motor backward
  digitalWrite(IN3, REVERSE_RIGHT_MOTOR ? HIGH : LOW);
  digitalWrite(IN4, REVERSE_RIGHT_MOTOR ? LOW : HIGH);

  analogWrite(ENA, MAX_SPEED);
  analogWrite(ENB, MAX_SPEED);
}

void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotors();

  Serial.println("Maximum Speed Motor Test");
  delay(2000);
}

void loop() {
  Serial.println("FORWARD - MAXIMUM PWM");
  moveForward();
  delay(2000);

  stopMotors();
  delay(1500);

  Serial.println("BACKWARD - MAXIMUM PWM");
  moveBackward();
  delay(2000);

  stopMotors();
  delay(3000);
}
