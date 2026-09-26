#pragma once
#include <windows.h>
#include <cstdint>

namespace sanctuary {
enum class WindowState { Other, Own, Unavailable, GameHidden, GameMinimized, GameEmpty, GameReady };
struct WindowProbe {
 HWND window{};
 DWORD pid{},error{};
 WindowState state{WindowState::Unavailable};
};
WindowProbe probeWindow(HWND);
WindowState gameWindowState(bool visible,bool minimized,int width,int height);
const char* windowStateName(WindowState);
bool discoveryEvent(DWORD event,LONG object,LONG child);
bool relevantWindowEvent(DWORD event,HWND source,HWND foreground,HWND tracked);

// One bounded recovery burst for a transient foreground state, never a steady poll.
class FocusRetry {
 public:
 void observe(WindowState state,std::uint64_t now);
 bool takeDue(std::uint64_t now);
 std::uint64_t next()const{return next_;}
 unsigned attempts()const{return attempts_;}
 private:
 bool started_{};
 unsigned attempts_{};
 std::uint64_t next_{},expires_{};
};
}
