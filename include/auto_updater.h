#pragma once

#include <Arduino.h>
#include <functional>

class AutoUpdater {
public:
  using StatusCallback = std::function<void(const String&)>;

  AutoUpdater(const char* currentVersion, const char* manifestUrl);
  void begin(StatusCallback callback);
  void loop();
  bool enabled() const;

private:
  bool checkForUpdate();
  bool install(const String& url, const String& expectedSha256);
  bool isNewer(const String& candidate) const;
  void report(const String& message) const;

  String currentVersion_;
  String manifestUrl_;
  StatusCallback callback_;
  uint32_t nextCheck_ = 0;
  bool connectionSeen_ = false;
};

