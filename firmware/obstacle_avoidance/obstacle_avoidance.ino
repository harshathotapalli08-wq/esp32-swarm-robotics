#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "secrets.h"   // copy secrets.example.h to secrets.h and edit

// ========== WIFI / UDP ==========
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;

IPAddress laptopIP(SECRET_LAPTOP_IP);
const unsigned int UDP_PORT = 4210;

WiFiUDP telemetryUDP;

// ========== SEND TELEMETRY ==========
void sendTelemetry(const String& message) {
  Serial.println(message);

  if (WiFi.status() == WL_CONNECTED) {
    telemetryUDP.beginPacket(laptopIP, UDP_PORT);
    telemetryUDP.print(message);
    telemetryUDP.endPacket();
  }
}

// ========== SERVO ==========
Servo myServo;
const int SERVO_PIN = 13;

int servoAngle = 20;
int servoDirection = 1;
unsigned long lastServoMove = 0;

const int START_ANGLE = 20;
const int END_ANGLE = 160;
const int SERVO_STEP = 5;
const unsigned long SERVO_INTERVAL_MS = 80;

// ========== TF-LUNA ==========
const int TF_RX = 16;
const int TF_TX = 17;

uint8_t frame[9];
int frameIndex = 0;

// ========== L298N ==========
const int ENA = 25;
const int IN1 = 26;
const int IN2 = 14;

const int ENB = 33;
const int IN3 = 32;
const int IN4 = 23;

const bool REVERSE_RIGHT_MOTOR = true;

// ========== SETTINGS ==========
const int OBSTACLE_CM = 30;
const int MOTOR_SPEED = 110;
const int REVERSE_SPEED = 100;
const int TURN_SPEED = 120;

const unsigned long FRONT_STALE_MS = 1800;
const unsigned long SIDE_STALE_MS = 6000;

const unsigned long REVERSE_TIME_MS = 450;
const unsigned long TURN_TIME_MS = 450;
const unsigned long SCAN_WAIT_MS = 2500;

// ========== DISTANCE READINGS ==========
int frontDistance = -1;
int leftDistance = -1;
int rightDistance = -1;

unsigned long frontTime = 0;
unsigned long leftTime = 0;
unsigned long rightTime = 0;

// ========== ROBOT STATES ==========
enum RobotState {
  WAIT_FOR_FRONT,
  FORWARD,
  REVERSE,
  WAIT_FOR_SIDE_SCAN,
  TURNING
};

RobotState state = WAIT_FOR_FRONT;
unsigned long stateStarted = 0;

bool preferLeft = true;

// ========== MOTOR CONTROL ==========
void setLeftMotor(int speed) {
  speed = constrain(speed, -255, 255);

  if (speed > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  } else if (speed < 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }

  analogWrite(ENA, abs(speed));
}

void setRightMotor(int speed) {
  speed = constrain(speed, -255, 255);

  bool forward = speed > 0;
  bool backward = speed < 0;

  if (REVERSE_RIGHT_MOTOR) {
    bool temp = forward;
    forward = backward;
    backward = temp;
  }

  if (forward) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else if (backward) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  }

  analogWrite(ENB, abs(speed));
}

void stopMotors() {
  setLeftMotor(0);
  setRightMotor(0);
}

void moveForward() {
  setLeftMotor(MOTOR_SPEED);
  setRightMotor(MOTOR_SPEED);
}

void moveBackward() {
  setLeftMotor(-REVERSE_SPEED);
  setRightMotor(-REVERSE_SPEED);
}

void turnLeft() {
  setLeftMotor(-TURN_SPEED);
  setRightMotor(TURN_SPEED);
}

void turnRight() {
  setLeftMotor(TURN_SPEED);
  setRightMotor(-TURN_SPEED);
}

// ========== HANDLE TF-LUNA READING ==========
void processReading(int distance, int strength) {
  unsigned long now = millis();

  String message = "SCAN," + String(now) + "," +
                   String(servoAngle) + "," +
                   String(distance) + "," +
                   String(strength);

  sendTelemetry(message);

  // Servo angle 90 degrees is assumed to face forward.
  if (servoAngle >= 75 && servoAngle <= 105) {
    frontDistance = distance;
    frontTime = now;
  } else if (servoAngle < 75) {
    leftDistance = distance;
    leftTime = now;
  } else {
    rightDistance = distance;
    rightTime = now;
  }
}

// ========== TF-LUNA UART PARSER ==========
void readTFData() {
  while (Serial2.available() > 0) {
    uint8_t b = Serial2.read();

    if (frameIndex == 0) {
      if (b == 0x59) {
        frame[frameIndex++] = b;
      }
      continue;
    }

    if (frameIndex == 1) {
      if (b == 0x59) {
        frame[frameIndex++] = b;
      } else {
        frameIndex = 0;
      }
      continue;
    }

    frame[frameIndex++] = b;

    if (frameIndex == 9) {
      uint8_t checksum = 0;

      for (int i = 0; i < 8; i++) {
        checksum += frame[i];
      }

      if (checksum == frame[8]) {
        int distance = frame[2] | (frame[3] << 8);
        int strength = frame[4] | (frame[5] << 8);

        if (distance > 0 && distance <= 800 && strength > 0) {
          processReading(distance, strength);
        }
      }

      frameIndex = 0;
    }
  }
}

// ========== SERVO SWEEP ==========
void updateServo() {
  unsigned long now = millis();

  if (now - lastServoMove < SERVO_INTERVAL_MS) {
    return;
  }

  lastServoMove = now;
  servoAngle += servoDirection * SERVO_STEP;

  if (servoAngle >= END_ANGLE) {
    servoAngle = END_ANGLE;
    servoDirection = -1;
  } else if (servoAngle <= START_ANGLE) {
    servoAngle = START_ANGLE;
    servoDirection = 1;
  }

  myServo.write(servoAngle);
}

// ========== CHOOSE TURN ==========
void chooseTurn() {
  unsigned long now = millis();

  bool leftFresh =
      leftDistance > 0 &&
      now - leftTime <= SIDE_STALE_MS;

  bool rightFresh =
      rightDistance > 0 &&
      now - rightTime <= SIDE_STALE_MS;

  bool leftClear =
      leftFresh && leftDistance > OBSTACLE_CM;

  bool rightClear =
      rightFresh && rightDistance > OBSTACLE_CM;

  if (leftClear && rightClear) {
    preferLeft = leftDistance >= rightDistance;
  } else if (leftClear) {
    preferLeft = true;
  } else if (rightClear) {
    preferLeft = false;
  } else if (leftFresh && rightFresh) {
    preferLeft = leftDistance >= rightDistance;
  } else {
    sendTelemetry("ACTION,NO_FRESH_SIDE_DATA");
    return;
  }

  if (preferLeft) {
    turnLeft();
    sendTelemetry("ACTION,TURN_LEFT");
  } else {
    turnRight();
    sendTelemetry("ACTION,TURN_RIGHT");
  }

  state = TURNING;
  stateStarted = now;
}

// ========== OBSTACLE AVOIDANCE ==========
void updateMovement() {
  unsigned long now = millis();

  switch (state) {
    case WAIT_FOR_FRONT:
      stopMotors();

      if (frontDistance > 0 &&
          now - frontTime <= FRONT_STALE_MS) {
        state = FORWARD;
        sendTelemetry("ACTION,FRONT_READING_READY");
      }
      break;

    case FORWARD:
      if (frontDistance <= 0 ||
          now - frontTime > FRONT_STALE_MS) {
        stopMotors();
        state = WAIT_FOR_FRONT;
        sendTelemetry("ACTION,WAITING_FOR_FRONT_READING");
      } else if (frontDistance <= OBSTACLE_CM) {
        stopMotors();
        moveBackward();

        state = REVERSE;
        stateStarted = now;

        sendTelemetry("ACTION,OBSTACLE_REVERSE");
      } else {
        moveForward();
      }
      break;

    case REVERSE:
      if (now - stateStarted >= REVERSE_TIME_MS) {
        stopMotors();

        state = WAIT_FOR_SIDE_SCAN;
        stateStarted = now;

        sendTelemetry("ACTION,SCANNING_FOR_TURN");
      }
      break;

    case WAIT_FOR_SIDE_SCAN:
      stopMotors();

      if (now - stateStarted >= SCAN_WAIT_MS) {
        chooseTurn();

        if (state == WAIT_FOR_SIDE_SCAN) {
          moveBackward();
          state = REVERSE;
          stateStarted = now;
          sendTelemetry("ACTION,REVERSE_AND_RESCAN");
        }
      }
      break;

    case TURNING:
      if (now - stateStarted >= TURN_TIME_MS) {
        stopMotors();

        state = WAIT_FOR_FRONT;
        sendTelemetry("ACTION,TURN_COMPLETE");
      }
      break;
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, TF_RX, TF_TX);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotors();

  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(servoAngle);

  Serial.println("SCAN,millis,angle,distance_cm,strength");
  Serial.println("Robot ready: scan + move + reverse + turn");

  // ========== WIFI CONNECTION ==========
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to phone hotspot...");

  for (int i = 0;
       i < 30 && WiFi.status() != WL_CONNECTED;
       i++) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WIFI CONNECTED!");
    Serial.print("ESP32 IP ADDRESS: ");
    Serial.println(WiFi.localIP());

    telemetryUDP.begin(4211);
    sendTelemetry("ACTION,WIFI_CONNECTED");
  } else {
    Serial.print("WIFI FAILED. Status code: ");
    Serial.println((int)WiFi.status());
    Serial.println("Robot will continue local operation.");
  }
}

// ========== MAIN LOOP ==========
void loop() {
  readTFData();
  updateServo();
  updateMovement();
}
