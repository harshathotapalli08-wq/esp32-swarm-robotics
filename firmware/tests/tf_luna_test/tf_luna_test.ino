
#include <Arduino.h>

// TF-Luna UART pins
#define TF_LUNA_RX 16   // ESP32 RX <- TF-Luna TX
#define TF_LUNA_TX 17   // ESP32 TX -> TF-Luna RX

HardwareSerial TFSerial(2);

uint8_t frame[9];
int frameIndex = 0;
unsigned long lastReadingTime = 0;

void setup() {
  Serial.begin(115200);

  TFSerial.begin(115200, SERIAL_8N1,
                 TF_LUNA_RX, TF_LUNA_TX);

  Serial.println("TF-Luna Continuous Distance Monitoring");
  Serial.println("--------------------------------------");
}

void loop() {
  while (TFSerial.available() > 0) {
    uint8_t b = TFSerial.read();

    // Find frame header: 0x59 0x59
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
      frameIndex = 0;

      uint8_t checksum = 0;
      for (int i = 0; i < 8; i++) {
        checksum += frame[i];
      }

      if (checksum == frame[8]) {
        uint16_t distance =
            frame[2] | (uint16_t(frame[3]) << 8);

        uint16_t strength =
            frame[4] | (uint16_t(frame[5]) << 8);

        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.print(" cm  |  ");
        Serial.print(distance / 100.0, 2);
        Serial.print(" m  |  Signal strength: ");
        Serial.println(strength);

        lastReadingTime = millis();
      }
    }
  }

  // Report if valid sensor frames stop arriving
  static unsigned long lastStatusTime = 0;

  if (millis() - lastStatusTime >= 2000) {
    if (lastReadingTime == 0 ||
        millis() - lastReadingTime > 2000) {
      Serial.println("Waiting for valid TF-Luna data...");
    }
    lastStatusTime = millis();
  }
}
