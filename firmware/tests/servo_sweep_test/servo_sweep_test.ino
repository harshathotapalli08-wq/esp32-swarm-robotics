
#include <ESP32Servo.h>

Servo lidarServo;

const int SERVO_PIN = 13;

void setup() {
  Serial.begin(115200);

  lidarServo.setPeriodHertz(50);
  lidarServo.attach(SERVO_PIN, 1000, 2000);

  lidarServo.write(90);
  delay(1000);
}

void loop() {
  // Sweep from left to right
  for (int angle = 0; angle <= 180; angle += 2) {
    lidarServo.write(angle);
    delay(40);
  }

  delay(500);

  // Sweep from right to left
  for (int angle = 180; angle >= 0; angle -= 2) {
    lidarServo.write(angle);
    delay(40);
  }

  delay(500);
}
