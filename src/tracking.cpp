#include "tracking.h"
#include <algorithm>
#include <cwchar>

namespace sanctuary {
WindowState gameWindowState(bool visible,bool minimized,int width,int height){
 if(!visible)return WindowState::GameHidden;
 if(minimized)return WindowState::GameMinimized;
 if(width<=0||height<=0)return WindowState::GameEmpty;
 return WindowState::GameReady;
}
WindowProbe probeWindow(HWND window){
 WindowProbe result;result.window=window;
 if(!window||!IsWindow(window))return result;
 if(!GetWindowThreadProcessId(window,&result.pid)){result.error=GetLastError();return result;}
 if(result.pid==GetCurrentProcessId()){result.state=WindowState::Own;return result;}
 auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,result.pid);
 if(!process){result.error=GetLastError();return result;}
 wchar_t image[32768]{};DWORD size=32768;
 auto queried=QueryFullProcessImageNameW(process,0,image,&size);
 if(!queried)result.error=GetLastError();
 CloseHandle(process);if(!queried)return result;
 const wchar_t* name=wcsrchr(image,L'\\');
 if(_wcsicmp(name?name+1:image,L"Diablo IV.exe")!=0){result.state=WindowState::Other;return result;}
 RECT area{};if(!GetClientRect(window,&area))result.error=GetLastError();
 result.state=gameWindowState(IsWindowVisible(window)!=FALSE,IsIconic(window)!=FALSE,area.right-area.left,area.bottom-area.top);
 return result;
}
const char* windowStateName(WindowState state){
 switch(state){
  case WindowState::Other:return "other-process";
  case WindowState::Own:return "overlay-window";
  case WindowState::Unavailable:return "foreground-unavailable";
  case WindowState::GameHidden:return "game-hidden";
  case WindowState::GameMinimized:return "game-minimized";
  case WindowState::GameEmpty:return "game-zero-size";
  case WindowState::GameReady:return "game-ready";
 }
 return "unknown";
}
bool discoveryEvent(DWORD event,LONG object,LONG child){
 return event==EVENT_SYSTEM_FOREGROUND||event==EVENT_SYSTEM_MINIMIZEEND||
  (event==EVENT_OBJECT_SHOW&&object==OBJID_WINDOW&&child==CHILDID_SELF);
}
bool relevantWindowEvent(DWORD event,HWND source,HWND foreground,HWND tracked){
 if(event==EVENT_SYSTEM_FOREGROUND)return true;
 if(!source)return false;
 if(event==EVENT_OBJECT_SHOW||event==EVENT_SYSTEM_MINIMIZEEND)return source==foreground||!foreground;
 return (event==EVENT_OBJECT_LOCATIONCHANGE||event==EVENT_OBJECT_DESTROY)&&source==tracked;
}
void FocusRetry::observe(WindowState state,std::uint64_t now){
 if(state==WindowState::GameReady||state==WindowState::Other||state==WindowState::Own){*this={};return;}
 if(!started_){started_=true;expires_=now+4000;}
 if(now>=expires_||attempts_>=6){next_=0;return;}
 constexpr unsigned delays[]={100,200,400,800,1000,1000};
 if(!next_)next_=std::min(now+delays[attempts_],expires_);
}
bool FocusRetry::takeDue(std::uint64_t now){
 if(!next_||now<next_)return false;
 next_=0;if(now>expires_)return false;
 ++attempts_;return true;
}
}
