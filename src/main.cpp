#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "app_config.h"
#include "auto_updater.h"
#include "cst820.h"
#include "display.h"
#include "wifi_touch_setup.h"

ClockDisplay display;
Cst820Touch touch;
WifiTouchSetup wifiSetup(display, touch);
AutoUpdater updater(APP_VERSION, OTA_MANIFEST_URL);
constexpr uint16_t Background = 0x0863;
constexpr uint16_t Muted = 0x9CF3;
constexpr uint16_t Accent = 0x4EBA;
bool ntpStarted = false;
uint32_t lastReconnect = 0;
uint32_t lastFrame = 0;
String drawnTime;
String drawnDate;
String drawnStatus;
bool frameInvalid = true;
bool drawnUpdateAvailable = false;
constexpr uint8_t BootButtonPin = 0;
constexpr uint32_t CalibrationHoldMs = 1500;
bool bootButtonPressed = false;
bool calibrationStarted = false;
uint32_t bootButtonPressedAt = 0;

void centerText(const char* text, int y, int size, uint16_t color) {
  display.setTextSize(size);
  display.setTextColor(color, Background);
  display.setTextDatum(lgfx::textdatum_t::top_center);
  display.drawString(text, 160, y);
}

void showMessage(const String& title, const String& detail = "") {
  display.fillRect(0, 65, 320, 175, Background);
  centerText(title.c_str(), 90, 2, TFT_WHITE);
  if (!detail.isEmpty()) centerText(detail.c_str(), 135, 1, Muted);
  frameInvalid = true;
}

void drawUpdateBadge(bool available) {
  if (!available) {
    display.fillRect(245, 8, 70, 36, Background);
    return;
  }
  constexpr uint16_t UpdateColor = 0xE986;
  display.fillRoundRect(245, 8, 70, 34, 5, UpdateColor);
  display.drawRoundRect(245, 8, 70, 34, 5, TFT_WHITE);
  display.setTextDatum(lgfx::textdatum_t::middle_center);
  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, UpdateColor);
  display.drawString("UPDATE", 280, 25);
}

void drawFrame() {
  display.fillScreen(Background);
  centerText("SEOUL CLOCK", 24, 2, Accent);
  display.drawFastHLine(24, 57, 272, Muted);
  display.fillRoundRect(253, 201, 62, 34, 4, 0x18E3);
  display.drawRoundRect(253, 201, 62, 34, 4, TFT_WHITE);
  display.setTextDatum(lgfx::textdatum_t::middle_center);
  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, 0x18E3);
  display.drawString("Wi-Fi", 284, 218);
  drawUpdateBadge(updater.updateAvailable());
  drawnUpdateAvailable = updater.updateAvailable();
  drawnTime = "";
  drawnDate = "";
  drawnStatus = "";
  frameInvalid = false;
}

void drawClock() {
  if (frameInvalid) drawFrame();
  const bool updateAvailable = updater.updateAvailable();
  if (drawnUpdateAvailable != updateAvailable) {
    drawUpdateBadge(updateAvailable);
    drawnUpdateAvailable = updateAvailable;
  }
  const time_t now = time(nullptr);
  const bool validTime = now >= 1704067200;
  String clockText;
  String dateText;
  if (validTime) {
    tm local{};
    localtime_r(&now, &local);
    char timeBuffer[9];
    char dateBuffer[32];
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &local);
    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d  %a", &local);
    clockText = timeBuffer;
    dateText = dateBuffer;
  } else {
    clockText = "--:--:--";
    dateText = "Waiting for time";
  }
  const String status = updater.updateAvailable()
    ? String("Update v") + updater.availableVersion() + " - tap UPDATE"
    : WiFi.status() != WL_CONNECTED ? "Wi-Fi connecting..."
    : !validTime ? "NTP syncing..." : "Wi-Fi connected / KST";

  if (clockText != drawnTime) {
    display.setTextDatum(lgfx::textdatum_t::middle_center);
    display.setTextSize(5);
    display.setTextColor(TFT_WHITE, Background);
    if (drawnTime.length() == clockText.length() && clockText.length() == 8) {
      const int totalWidth = display.textWidth(clockText);
      const int left = (320 - totalWidth) / 2;
      int prefixWidth = 0;
      for (uint8_t i = 0; i < clockText.length(); ++i) {
        const String character = clockText.substring(i, i + 1);
        const String oldCharacter = drawnTime.substring(i, i + 1);
        const int characterWidth = display.textWidth(character);
        if (character != oldCharacter) {
          const int x = left + prefixWidth;
          display.fillRect(x, 76, characterWidth, 60, Background);
          display.drawString(character, x + characterWidth / 2, 106);
        }
        prefixWidth += characterWidth;
      }
    } else {
      display.fillRect(0, 76, 320, 60, Background);
      display.drawString(clockText, 160, 106);
    }
    drawnTime = clockText;
  }
  if (dateText != drawnDate) {
    display.fillRect(0, 145, 320, 32, Background);
    centerText(dateText.c_str(), 151, 2, Muted);
    drawnDate = dateText;
  }
  if (status != drawnStatus) {
    display.fillRect(0, 201, 248, 38, Background);
    display.setTextDatum(lgfx::textdatum_t::middle_left);
    display.setTextSize(1);
    display.setTextColor(Accent, Background);
    display.drawString(status, 10, 220);
    drawnStatus = status;
  }
}

void drawCalibrationTarget(int16_t x, int16_t y, uint8_t step) {
  display.fillScreen(Background);
  display.setTextDatum(lgfx::textdatum_t::middle_center);
  display.setTextSize(2);
  display.setTextColor(Accent, Background);
  display.drawString("Touch calibration", 160, 105);
  display.setTextSize(1);
  display.setTextColor(Muted, Background);
  display.drawString((String("Tap the cross  ") + String(step) + "/4").c_str(), 160, 130);
  display.drawCircle(x, y, 13, TFT_WHITE);
  display.drawCircle(x, y, 4, Accent);
  display.drawFastHLine(x - 19, y, 39, Accent);
  display.drawFastVLine(x, y - 19, 39, Accent);
}

void runTouchCalibration() {
  constexpr int16_t targetX[4] = {20, 299, 299, 20};
  constexpr int16_t targetY[4] = {20, 20, 219, 219};
  int16_t rawX[4]{};
  int16_t rawY[4]{};

  Serial.println("Touch calibration started from BOOT button.");
  for (uint8_t i = 0; i < 4; ++i) {
    drawCalibrationTarget(targetX[i], targetY[i], i + 1);
    const uint32_t started = millis();
    bool captured = false;
    while (millis() - started < 30000) {
      if (touch.rawTap(rawX[i], rawY[i])) {
        captured = true;
        Serial.printf("Calibration point %u: rawX=%d rawY=%d\n", i + 1, rawX[i], rawY[i]);
        break;
      }
      delay(10);
    }
    if (!captured) {
      showMessage("Calibration timed out", "Hold BOOT to try again");
      delay(1000);
      frameInvalid = true;
      drawnTime = "";
      drawnDate = "";
      drawnStatus = "";
      drawClock();
      Serial.println("Touch calibration timed out.");
      return;
    }
  }

  const bool saved = touch.saveCalibration(rawX, rawY, targetX, targetY);
  showMessage(saved ? "Calibration saved" : "Calibration failed",
              saved ? "Touch points were stored" : "Tap points again");
  Serial.println(saved ? "Touch calibration saved to NVS." : "Touch calibration rejected.");
  delay(1000);
  frameInvalid = true;
  drawnTime = "";
  drawnDate = "";
  drawnStatus = "";
  drawClock();
}

bool checkBootButton() {
  if (digitalRead(BootButtonPin) != LOW) {
    bootButtonPressed = false;
    calibrationStarted = false;
    return false;
  }
  if (!bootButtonPressed) {
    bootButtonPressed = true;
    bootButtonPressedAt = millis();
  }
  if (!calibrationStarted && millis() - bootButtonPressedAt >= CalibrationHoldMs) {
    calibrationStarted = true;
    runTouchCalibration();
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BootButtonPin, INPUT_PULLUP);
  display.init();
  display.setRotation(1);
  pinMode(ClockDisplay::Backlight, OUTPUT);
  digitalWrite(ClockDisplay::Backlight, HIGH);
  touch.begin();
  Serial.printf("CST820 chip ID: 0x%02X (expected 0xB7)\n", touch.chipId());
  drawFrame();
  // Set KST immediately. NTP starts once a network connection is available.
  setenv("TZ", "KST-9", 1);
  tzset();
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  const bool connected = wifiSetup.connectSaved(12000) || wifiSetup.run();
  Serial.println(connected ? "Wi-Fi connected." : "Wi-Fi setup cancelled.");
  lastReconnect = millis();
  updater.begin([](const String& status) {
    if (status.startsWith("OTA: update available:")) {
      drawUpdateBadge(true);
      drawnUpdateAvailable = true;
      display.fillRect(0, 201, 248, 38, Background);
      display.setTextDatum(lgfx::textdatum_t::middle_left);
      display.setTextSize(1);
      display.setTextColor(0xE986, Background);
      display.drawString((String("Update v") + updater.availableVersion() + " - tap UPDATE").c_str(), 10, 220);
      drawnStatus = String("Update v") + updater.availableVersion() + " - tap UPDATE";
    } else if (status.startsWith("OTA: downloading") || status.startsWith("OTA: update complete") ||
               status.startsWith("OTA: update failed")) {
      showMessage("Firmware update", status);
    }
  });
  drawFrame();
  drawClock();
}

void loop() {
  const uint32_t tick = millis();
  if (WiFi.status() == WL_CONNECTED && !ntpStarted) {
    configTzTime("KST-9", "pool.ntp.org", "time.google.com");
    ntpStarted = true;
    Serial.println("Wi-Fi connected; NTP synchronization started.");
  }
  if (WiFi.status() != WL_CONNECTED && tick - lastReconnect >= 30000) {
    lastReconnect = tick;
    WiFi.reconnect();
  }
  if (checkBootButton()) {
    delay(10);
    return;
  }
  updater.loop();
  int16_t touchX, touchY;
  if (touch.tap(touchX, touchY)) {
    if (updater.updateAvailable() && touchX >= 245 && touchY <= 45) {
      updater.installAvailableUpdate();
      frameInvalid = true;
      drawClock();
    } else if (touchX >= 253 && touchY >= 201) {
      const bool connected = wifiSetup.run();
      Serial.println(connected ? "Wi-Fi settings updated." : "Wi-Fi settings cancelled.");
      ntpStarted = false;
      lastReconnect = millis();
      drawFrame();
      drawClock();
    }
  }
  if (tick - lastFrame >= 1000) {
    lastFrame = tick;
    drawClock();
  }
  delay(10);
}
