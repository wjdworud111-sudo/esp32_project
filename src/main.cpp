#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "app_config.h"
#include "auto_updater.h"
#include "cst820.h"
#include "display.h"
#include "wifi_touch_setup.h"

ClockDisplay display;
LGFX_Sprite clockSprite(&display);
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
bool clockSpriteReady = false;

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
  drawnTime = "";
  drawnDate = "";
  drawnStatus = "";
  frameInvalid = false;
}

void drawClock() {
  if (frameInvalid) drawFrame();
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
  const String status = WiFi.status() != WL_CONNECTED ? "Wi-Fi connecting..."
    : !validTime ? "NTP syncing..." : "Wi-Fi connected / KST";

  // Render the changing seconds into RAM first, then transfer the completed
  // area in one operation. The LCD never sees a blank intermediate frame.
  if (clockText != drawnTime) {
    if (clockSpriteReady) {
      clockSprite.fillSprite(Background);
      clockSprite.setTextDatum(lgfx::textdatum_t::middle_center);
      clockSprite.setTextSize(5);
      clockSprite.setTextColor(TFT_WHITE, Background);
      clockSprite.drawString(clockText, 150, 30);
      clockSprite.pushSprite(10, 76);
    } else {
      centerText(clockText.c_str(), 87, 5, TFT_WHITE);
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

void setup() {
  Serial.begin(115200);
  display.init();
  display.setRotation(1);
  clockSprite.setColorDepth(16);
  clockSpriteReady = clockSprite.createSprite(300, 60) != nullptr;
  if (!clockSpriteReady) {
    Serial.println("Clock sprite allocation failed.");
  }
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
  updater.begin([](const String& status) { showMessage(status); });
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
  updater.loop();
  int16_t touchX, touchY;
  if (touch.tap(touchX, touchY) && touchX >= 253 && touchY >= 201) {
    const bool connected = wifiSetup.run();
    Serial.println(connected ? "Wi-Fi settings updated." : "Wi-Fi settings cancelled.");
    ntpStarted = false;
    lastReconnect = millis();
    drawFrame();
    drawClock();
  }
  if (tick - lastFrame >= 1000) {
    lastFrame = tick;
    drawClock();
  }
  delay(10);
}
