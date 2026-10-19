#pragma once

#include <Arduino.h>
#include <Wire.h>

// Pin assignment and register format verified against the JC2432W328C
// capacitive-touch example:
// https://github.com/pay191/DIY_Malls-JC2432W328C_Tests/tree/main/BigYellowDisplay_3_CapcitiveTouch
class Cst820Touch {
 public:
  static constexpr uint8_t Address = 0x15;
  static constexpr int Sda = 33;
  static constexpr int Scl = 32;
  static constexpr int Reset = 25;
  static constexpr int Interrupt = 21;

  void begin() {
    pinMode(Reset, OUTPUT);
    pinMode(Interrupt, INPUT_PULLUP);
    digitalWrite(Reset, LOW);
    delay(10);
    digitalWrite(Reset, HIGH);
    delay(100);
    Wire.begin(Sda, Scl);
  }

  uint8_t chipId() {
    Wire.beginTransmission(Address);
    Wire.write(0xA7);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(Address, uint8_t(1)) != 1) {
      return 0;
    }
    return Wire.read();
  }

  bool read(int16_t& screenX, int16_t& screenY) {
    Wire.beginTransmission(Address);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(Address, uint8_t(7)) != 7) {
      return false;
    }
    uint8_t data[7];
    for (uint8_t& value : data) value = Wire.read();
    if ((data[2] & 0x0F) == 0) return false;

    const int16_t rawX = ((data[3] & 0x0F) << 8) | data[4];
    const int16_t rawY = ((data[5] & 0x0F) << 8) | data[6];
    screenX = constrain(rawY, 0, 319);
    screenY = constrain(240 - rawX, 0, 239);
    return true;
  }

  bool tap(int16_t& x, int16_t& y) {
    if (!read(x, y)) return false;
    const uint32_t started = millis();
    int16_t ignoredX, ignoredY;
    while (read(ignoredX, ignoredY) && millis() - started < 1200) delay(15);
    delay(35);
    return true;
  }
};
