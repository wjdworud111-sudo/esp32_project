#pragma once

#include <Arduino.h>
#include <Preferences.h>
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

  struct Calibration {
    int16_t rawXTop;
    int16_t rawXBottom;
    int16_t rawYLeft;
    int16_t rawYRight;
    int16_t screenYTop;
    int16_t screenYBottom;
    int16_t screenXLeft;
    int16_t screenXRight;
    uint8_t valid;
  };

  void begin() {
    pinMode(Reset, OUTPUT);
    pinMode(Interrupt, INPUT_PULLUP);
    digitalWrite(Reset, LOW);
    delay(10);
    digitalWrite(Reset, HIGH);
    delay(100);
    Wire.begin(Sda, Scl);
    Preferences preferences;
    if (preferences.begin("touchcal", true)) {
      if (preferences.getBytesLength("points") == sizeof(calibration_)) {
        preferences.getBytes("points", &calibration_, sizeof(calibration_));
      }
      preferences.end();
    }
    calibrated_ = calibration_.valid == 1 &&
        abs(calibration_.rawXBottom - calibration_.rawXTop) >= 50 &&
        abs(calibration_.rawYRight - calibration_.rawYLeft) >= 50;
    Serial.printf("Touch calibration: %s\n", calibrated_ ? "loaded" : "default mapping");
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
    int16_t rawX, rawY;
    if (!readRaw(rawX, rawY)) return false;
    if (calibrated_) {
      screenX = constrain(scale(rawY, calibration_.rawYLeft, calibration_.rawYRight,
                                calibration_.screenXLeft, calibration_.screenXRight), 0, 319);
      screenY = constrain(scale(rawX, calibration_.rawXTop, calibration_.rawXBottom,
                                calibration_.screenYTop, calibration_.screenYBottom), 0, 239);
    } else {
      screenX = constrain(rawY, 0, 319);
      screenY = constrain(240 - rawX, 0, 239);
    }
    return true;
  }

  bool readRaw(int16_t& rawX, int16_t& rawY) {
    Wire.beginTransmission(Address);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(Address, uint8_t(7)) != 7) {
      return false;
    }
    uint8_t data[7];
    for (uint8_t& value : data) value = Wire.read();
    if ((data[2] & 0x0F) == 0) return false;

    rawX = ((data[3] & 0x0F) << 8) | data[4];
    rawY = ((data[5] & 0x0F) << 8) | data[6];
    return true;
  }

  bool rawTap(int16_t& rawX, int16_t& rawY) {
    if (!readRaw(rawX, rawY)) return false;
    const uint32_t started = millis();
    int16_t ignoredX, ignoredY;
    while (millis() - started < 5000) {
      if (!readRaw(ignoredX, ignoredY)) {
        delay(35);
        return true;
      }
      delay(15);
    }
    return false;
  }

  bool tap(int16_t& x, int16_t& y) {
    int16_t rawX, rawY;
    if (!rawTap(rawX, rawY)) return false;
    if (calibrated_) {
      x = constrain(scale(rawY, calibration_.rawYLeft, calibration_.rawYRight,
                          calibration_.screenXLeft, calibration_.screenXRight), 0, 319);
      y = constrain(scale(rawX, calibration_.rawXTop, calibration_.rawXBottom,
                          calibration_.screenYTop, calibration_.screenYBottom), 0, 239);
    } else {
      x = constrain(rawY, 0, 319);
      y = constrain(240 - rawX, 0, 239);
    }
    return true;
  }

  bool saveCalibration(const int16_t rawX[4], const int16_t rawY[4],
                       const int16_t screenX[4], const int16_t screenY[4]) {
    Calibration next{};
    next.rawXTop = (rawX[0] + rawX[1]) / 2;
    next.rawXBottom = (rawX[2] + rawX[3]) / 2;
    next.rawYLeft = (rawY[0] + rawY[3]) / 2;
    next.rawYRight = (rawY[1] + rawY[2]) / 2;
    next.screenYTop = (screenY[0] + screenY[1]) / 2;
    next.screenYBottom = (screenY[2] + screenY[3]) / 2;
    next.screenXLeft = (screenX[0] + screenX[3]) / 2;
    next.screenXRight = (screenX[1] + screenX[2]) / 2;
    next.valid = 1;
    if (abs(next.rawXBottom - next.rawXTop) < 50 ||
        abs(next.rawYRight - next.rawYLeft) < 50) return false;

    Preferences preferences;
    if (!preferences.begin("touchcal", false)) return false;
    const bool saved = preferences.putBytes("points", &next, sizeof(next)) == sizeof(next);
    preferences.end();
    if (saved) {
      calibration_ = next;
      calibrated_ = true;
    }
    return saved;
  }

 private:
  Calibration calibration_{};
  bool calibrated_ = false;

  static int16_t scale(int16_t value, int16_t inStart, int16_t inEnd,
                       int16_t outStart, int16_t outEnd) {
    if (inStart == inEnd) return outStart;
    return outStart + static_cast<int32_t>(value - inStart) * (outEnd - outStart) /
        (inEnd - inStart);
  }
};
