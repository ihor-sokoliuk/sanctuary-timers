#pragma once
#include "core.h"
#include "clock.h"
#include "tracking.h"
#include <filesystem>
#include <atomic>
#include <windows.h>
namespace sanctuary {
Time utcNow();
Millis utcMillis();
struct ClockResponse { std::optional<ClockSample> sample; std::string error; };
ClockResponse fetchClock(const std::atomic_bool* running=nullptr);
Preferences loadPreferences(const std::filesystem::path&);
bool savePreferences(const std::filesystem::path&, Preferences);
std::array<Record,3> loadCache(const std::filesystem::path&,Time now);
bool saveCache(const std::filesystem::path&,const std::array<Record,3>&);
struct Response { std::optional<Record> record; Time retryAfter{}; unsigned status{}; std::string error; };
Response fetchRecord(Kind,Time now,const std::atomic_bool* allowed=nullptr);
bool registerStartup(const std::filesystem::path& exe, Preferences& preferences);
std::filesystem::path executablePath();
bool isGameWindow(HWND);
Box gameBounds(HWND);
}
