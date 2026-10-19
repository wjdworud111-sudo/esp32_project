#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "cst820.h"
#include "display.h"

class WifiTouchSetup {
 public:
  WifiTouchSetup(ClockDisplay& display, Cst820Touch& touch)
      : display_(display), touch_(touch) {}

  bool connectSaved(uint32_t timeoutMs);
  bool run();

 private:
  struct Network {
    String ssid;
    int32_t rssi;
    bool open;
  };

  static constexpr uint8_t MaxNetworks = 16;
  ClockDisplay& display_;
  Cst820Touch& touch_;
  Network networks_[MaxNetworks];
  uint8_t networkCount_ = 0;
  uint8_t page_ = 0;

  void title(const String& text);
  void button(int x, int y, int w, int h, const String& text, uint16_t color);
  bool hit(int16_t x, int16_t y, int bx, int by, int bw, int bh) const;
  void scan();
  void drawNetworks();
  bool keyboard(const String& ssid, String& password);
  void drawKeyboard(const String& ssid, const String& password, uint8_t mode);
  bool connect(const Network& network, const String& password);
};
