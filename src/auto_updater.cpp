#include "auto_updater.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>

#include "github_root_ca.h"

namespace {
constexpr uint32_t InitialCheckDelayMs = 30000;
constexpr uint32_t CheckIntervalMs = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t RetryIntervalMs = 15UL * 60UL * 1000UL;
constexpr uint32_t DownloadIdleTimeoutMs = 15000;

int versionPart(const String& version, int wantedPart) {
  int part = 0;
  int value = 0;
  bool foundDigit = false;
  for (size_t i = 0; i <= version.length(); ++i) {
    const char c = i < version.length() ? version[i] : '.';
    if (c >= '0' && c <= '9') {
      value = value * 10 + (c - '0');
      foundDigit = true;
    } else if (c == '.') {
      if (part == wantedPart) return foundDigit ? value : 0;
      ++part;
      value = 0;
      foundDigit = false;
    } else if (foundDigit) {
      if (part == wantedPart) return value;
      ++part;
      value = 0;
      foundDigit = false;
    }
  }
  return 0;
}

String hexDigest(const uint8_t* digest, size_t length) {
  static const char Hex[] = "0123456789abcdef";
  String result;
  result.reserve(length * 2);
  for (size_t i = 0; i < length; ++i) {
    result += Hex[digest[i] >> 4];
    result += Hex[digest[i] & 0x0f];
  }
  return result;
}
}  // namespace

AutoUpdater::AutoUpdater(const char* currentVersion, const char* manifestUrl)
    : currentVersion_(currentVersion), manifestUrl_(manifestUrl) {}

void AutoUpdater::begin(StatusCallback callback) {
  callback_ = callback;
  if (enabled()) {
    Serial.printf("Automatic OTA enabled: %s\n", manifestUrl_.c_str());
  } else {
    Serial.println("Automatic OTA disabled: OTA_MANIFEST_URL is empty.");
  }
}

bool AutoUpdater::enabled() const {
  return !manifestUrl_.isEmpty();
}

void AutoUpdater::loop() {
  if (!enabled() || WiFi.status() != WL_CONNECTED || time(nullptr) < 1704067200) {
    connectionSeen_ = false;
    return;
  }

  const uint32_t now = millis();
  if (!connectionSeen_) {
    connectionSeen_ = true;
    nextCheck_ = now + InitialCheckDelayMs;
    return;
  }

  if (static_cast<int32_t>(now - nextCheck_) < 0) return;
  const bool succeeded = checkForUpdate();
  nextCheck_ = millis() + (succeeded ? CheckIntervalMs : RetryIntervalMs);
}

bool AutoUpdater::isNewer(const String& candidate) const {
  for (int part = 0; part < 3; ++part) {
    const int remote = versionPart(candidate, part);
    const int local = versionPart(currentVersion_, part);
    if (remote != local) return remote > local;
  }
  return false;
}

void AutoUpdater::report(const String& message) const {
  Serial.println(message);
  if (callback_) callback_(message);
}

bool AutoUpdater::checkForUpdate() {
  report("OTA: checking for update");

  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(client, manifestUrl_)) {
    report("OTA: invalid manifest URL");
    return false;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    report("OTA: manifest HTTP " + String(status));
    http.end();
    return false;
  }

  JsonDocument document;
  const DeserializationError jsonError = deserializeJson(document, http.getStream());
  http.end();
  if (jsonError) {
    report("OTA: invalid version.json");
    return false;
  }

  const String version = document["version"] | "";
  const String firmwareUrl = document["url"] | "";
  String sha256 = document["sha256"] | "";
  sha256.toLowerCase();
  if (version.isEmpty() || firmwareUrl.isEmpty() || sha256.length() != 64) {
    report("OTA: incomplete version.json");
    return false;
  }
  if (!isNewer(version)) {
    report("OTA: version " + currentVersion_ + " is current");
    return true;
  }

  report("OTA: updating to " + version);
  return install(firmwareUrl, sha256);
}

bool AutoUpdater::install(const String& url, const String& expectedSha256) {
  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA);
  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) {
    report("OTA: invalid firmware URL");
    return false;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    report("OTA: firmware HTTP " + String(status));
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength <= 0) {
    report("OTA: firmware size is missing");
    http.end();
    return false;
  }
  if (!Update.begin(static_cast<size_t>(contentLength))) {
    report("OTA: insufficient flash space");
    http.end();
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts_ret(&sha, 0);

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[4096];
  size_t total = 0;
  uint32_t lastDataAt = millis();
  int lastPercent = -1;
  bool failed = false;

  while (http.connected() && total < static_cast<size_t>(contentLength)) {
    const size_t available = stream->available();
    if (!available) {
      if (millis() - lastDataAt > DownloadIdleTimeoutMs) {
        failed = true;
        break;
      }
      delay(1);
      continue;
    }

    const size_t wanted = min(available, sizeof(buffer));
    const int received = stream->readBytes(buffer, wanted);
    if (received <= 0 || Update.write(buffer, received) != static_cast<size_t>(received)) {
      failed = true;
      break;
    }
    mbedtls_sha256_update_ret(&sha, buffer, received);
    total += received;
    lastDataAt = millis();

    if (contentLength > 0) {
      const int percent = static_cast<int>((total * 100U) / contentLength);
      if (percent / 10 != lastPercent / 10) {
        lastPercent = percent;
        report("OTA: downloading " + String(percent) + "%");
      }
    }
  }

  uint8_t digest[32];
  mbedtls_sha256_finish_ret(&sha, digest);
  mbedtls_sha256_free(&sha);
  http.end();

  if (total != static_cast<size_t>(contentLength)) failed = true;
  if (failed || total == 0) {
    Update.abort();
    report("OTA: download failed");
    return false;
  }

  if (!hexDigest(digest, sizeof(digest)).equalsIgnoreCase(expectedSha256)) {
    Update.abort();
    report("OTA: SHA-256 mismatch");
    return false;
  }
  if (!Update.end(true)) {
    report("OTA: flash verification failed");
    return false;
  }

  report("OTA: update complete, restarting");
  delay(1200);
  ESP.restart();
  return true;
}
