#include "wifi_touch_setup.h"

namespace {
constexpr uint16_t Background = 0x0863;
constexpr uint16_t Panel = 0x18E3;
constexpr uint16_t Muted = 0x9CF3;
constexpr uint16_t Accent = 0x4EBA;
constexpr uint16_t Danger = 0xF986;
constexpr uint8_t RowsPerPage = 5;

const char* const LowerRows[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
const char* const UpperRows[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
const char* const SymbolRows[] = {"1234567890", "!@#$%^&*()", "-_=+[]{}", ";:'\",.<>/?"};
}

void WifiTouchSetup::title(const String& text) {
  display_.fillScreen(Background);
  display_.setTextDatum(lgfx::textdatum_t::top_center);
  display_.setTextSize(2);
  display_.setTextColor(TFT_WHITE, Background);
  display_.drawString(text, 160, 8);
  display_.drawFastHLine(12, 32, 296, Muted);
}

void WifiTouchSetup::button(int x, int y, int w, int h, const String& text, uint16_t color) {
  display_.fillRoundRect(x, y, w, h, 4, color);
  display_.drawRoundRect(x, y, w, h, 4, TFT_WHITE);
  display_.setTextDatum(lgfx::textdatum_t::middle_center);
  display_.setTextSize(1);
  display_.setTextColor(TFT_WHITE, color);
  display_.drawString(text, x + w / 2, y + h / 2);
}

bool WifiTouchSetup::hit(int16_t x, int16_t y, int bx, int by, int bw, int bh) const {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

bool WifiTouchSetup::connectSaved(uint32_t timeoutMs) {
  title("Wi-Fi connecting");
  display_.setTextDatum(lgfx::textdatum_t::middle_center);
  display_.setTextSize(1);
  display_.setTextColor(Muted, Background);
  display_.drawString("Trying saved network...", 160, 110);
  WiFi.begin();
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < timeoutMs) delay(100);
  return WiFi.status() == WL_CONNECTED;
}

void WifiTouchSetup::scan() {
  title("Select Wi-Fi");
  display_.setTextDatum(lgfx::textdatum_t::middle_center);
  display_.setTextSize(1);
  display_.setTextColor(Muted, Background);
  display_.drawString("Scanning nearby networks...", 160, 112);
  networkCount_ = 0;
  page_ = 0;
  const int found = WiFi.scanNetworks(false, true);
  for (int i = 0; i < found && networkCount_ < MaxNetworks; ++i) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    bool duplicate = false;
    for (uint8_t n = 0; n < networkCount_; ++n) {
      if (networks_[n].ssid == ssid) {
        duplicate = true;
        if (WiFi.RSSI(i) > networks_[n].rssi) networks_[n].rssi = WiFi.RSSI(i);
        break;
      }
    }
    if (!duplicate) {
      networks_[networkCount_++] = {ssid, WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN};
    }
  }
  WiFi.scanDelete();
}

void WifiTouchSetup::drawNetworks() {
  title("Select Wi-Fi");
  const uint8_t start = page_ * RowsPerPage;
  for (uint8_t row = 0; row < RowsPerPage; ++row) {
    const uint8_t index = start + row;
    const int y = 38 + row * 31;
    if (index >= networkCount_) break;
    display_.fillRoundRect(8, y, 304, 28, 3, Panel);
    display_.setTextDatum(lgfx::textdatum_t::middle_left);
    display_.setTextSize(1);
    display_.setTextColor(TFT_WHITE, Panel);
    String label = networks_[index].ssid;
    if (label.length() > 27) label = label.substring(0, 24) + "...";
    display_.drawString(label, 15, y + 14);
    display_.setTextDatum(lgfx::textdatum_t::middle_right);
    display_.setTextColor(networks_[index].open ? Accent : Muted, Panel);
    display_.drawString(networks_[index].open ? "OPEN" : String(networks_[index].rssi) + " dBm", 304, y + 14);
  }
  button(8, 202, 67, 32, "Cancel", Danger);
  button(79, 202, 67, 32, "Rescan", Panel);
  button(150, 202, 77, 32, "< Prev", Panel);
  button(231, 202, 81, 32, "Next >", Panel);
  if (networkCount_ == 0) {
    display_.setTextDatum(lgfx::textdatum_t::middle_center);
    display_.setTextColor(Muted, Background);
    display_.drawString("No network found", 160, 112);
  }
}

void WifiTouchSetup::drawKeyboard(const String& ssid, const String& password, uint8_t mode) {
  display_.fillScreen(Background);
  display_.setTextDatum(lgfx::textdatum_t::middle_left);
  display_.setTextSize(1);
  display_.setTextColor(Accent, Background);
  String name = ssid.length() > 28 ? ssid.substring(0, 25) + "..." : ssid;
  display_.drawString(name, 6, 12);
  display_.fillRoundRect(5, 25, 310, 32, 3, Panel);
  display_.setTextColor(TFT_WHITE, Panel);
  String visible = password;
  if (visible.length() > 37) visible = "..." + visible.substring(visible.length() - 34);
  display_.drawString(visible, 10, 41);
  display_.setTextDatum(lgfx::textdatum_t::middle_right);
  display_.setTextColor(Muted, Panel);
  display_.drawString(String(password.length()) + "/63", 309, 41);

  const char* const* rows = mode == 0 ? LowerRows : mode == 1 ? UpperRows : SymbolRows;
  for (uint8_t row = 0; row < 4; ++row) {
    const int count = strlen(rows[row]);
    const int keyW = 30;
    const int startX = (320 - count * keyW) / 2;
    const int y = 62 + row * 34;
    for (int col = 0; col < count; ++col) {
      button(startX + col * keyW + 1, y, keyW - 2, 30, String(rows[row][col]), Panel);
    }
  }
  button(3, 199, 51, 38, mode == 1 ? "abc" : "Shift", Panel);
  button(57, 199, 48, 38, mode == 2 ? "ABC" : "?123", Panel);
  button(108, 199, 79, 38, "Space", Panel);
  button(190, 199, 58, 38, "Back", Danger);
  button(251, 199, 66, 38, "Connect", Accent);
}

bool WifiTouchSetup::keyboard(const String& ssid, String& password) {
  uint8_t mode = 0;
  while (true) {
    drawKeyboard(ssid, password, mode);
    int16_t x, y;
    while (!touch_.tap(x, y)) delay(10);
    if (y >= 62 && y < 198) {
      const uint8_t row = (y - 62) / 34;
      if (row < 4) {
        const char* const* rows = mode == 0 ? LowerRows : mode == 1 ? UpperRows : SymbolRows;
        const int count = strlen(rows[row]);
        const int keyW = 30;
        const int startX = (320 - count * keyW) / 2;
        const int col = (x - startX) / keyW;
        if (x >= startX && col >= 0 && col < count && password.length() < 63) password += rows[row][col];
      }
    } else if (hit(x, y, 3, 199, 51, 38)) {
      mode = mode == 1 ? 0 : 1;
    } else if (hit(x, y, 57, 199, 48, 38)) {
      mode = mode == 2 ? 1 : 2;
    } else if (hit(x, y, 108, 199, 79, 38) && password.length() < 63) {
      password += ' ';
    } else if (hit(x, y, 190, 199, 58, 38)) {
      if (!password.isEmpty()) password.remove(password.length() - 1);
      else return false;
    } else if (hit(x, y, 251, 199, 66, 38)) {
      return true;
    }
  }
}

bool WifiTouchSetup::connect(const Network& network, const String& password) {
  title("Wi-Fi connecting");
  display_.setTextDatum(lgfx::textdatum_t::middle_center);
  display_.setTextSize(1);
  display_.setTextColor(TFT_WHITE, Background);
  display_.drawString(network.ssid, 160, 91);
  display_.setTextColor(Muted, Background);
  display_.drawString("Please wait...", 160, 125);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.begin(network.ssid.c_str(), network.open ? nullptr : password.c_str());
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) delay(100);
  if (WiFi.status() == WL_CONNECTED) return true;
  display_.setTextColor(Danger, Background);
  display_.drawString("Connection failed - check password", 160, 165);
  delay(1800);
  return false;
}

bool WifiTouchSetup::run() {
  scan();
  while (true) {
    drawNetworks();
    int16_t x, y;
    while (!touch_.tap(x, y)) delay(10);
    if (hit(x, y, 8, 202, 67, 32)) return false;
    if (hit(x, y, 79, 202, 67, 32)) {
      scan();
      continue;
    }
    const uint8_t pages = max<uint8_t>(1, (networkCount_ + RowsPerPage - 1) / RowsPerPage);
    if (hit(x, y, 150, 202, 77, 32)) {
      if (page_ > 0) --page_;
      continue;
    }
    if (hit(x, y, 231, 202, 81, 32)) {
      if (page_ + 1 < pages) ++page_;
      continue;
    }
    if (x >= 8 && x < 312 && y >= 38 && y < 193) {
      const uint8_t row = (y - 38) / 31;
      const uint8_t index = page_ * RowsPerPage + row;
      if (index < networkCount_) {
        String password;
        if ((networks_[index].open || keyboard(networks_[index].ssid, password)) &&
            connect(networks_[index], password)) return true;
      }
    }
  }
}
