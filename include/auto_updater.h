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
  bool updateAvailable() const;
  const String& availableVersion() const;
  bool installAvailableUpdate();

private:
  bool checkForUpdate();
  bool install(const String& url, const String& expectedSha256);
  bool isNewer(const String& candidate) const;
  void report(const String& message) const;
  void notify(const String& message) const;

  String currentVersion_;
  String manifestUrl_;
  StatusCallback callback_;
  uint32_t nextCheck_ = 0;
  bool connectionSeen_ = false;
  String availableVersion_;
  String availableUrl_;
  String availableSha256_;
};

