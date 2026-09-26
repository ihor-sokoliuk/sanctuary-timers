#include "tracking.h"
#include <iostream>
#include <stdexcept>
using namespace sanctuary;
#define CHECK(v) if(!(v))throw std::runtime_error(#v)
int main(){
 int passed=0,failed=0;
 auto test=[&](const char* name,auto body){try{body();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
 auto game=reinterpret_cast<HWND>(1),other=reinterpret_cast<HWND>(2),replacement=reinterpret_cast<HWND>(3);
 test("shown game is discovered without a second foreground event",[&]{
  CHECK(discoveryEvent(EVENT_OBJECT_SHOW,OBJID_WINDOW,CHILDID_SELF));
  CHECK(relevantWindowEvent(EVENT_OBJECT_SHOW,game,game,nullptr));
  FocusRetry retry;retry.observe(WindowState::GameHidden,1000);CHECK(retry.next()>1000);
  retry.observe(WindowState::GameReady,1050);CHECK(retry.next()==0);CHECK(!retry.takeDue(2000));
 });
 test("restored game and replacement window trigger discovery",[&]{
  CHECK(discoveryEvent(EVENT_SYSTEM_MINIMIZEEND,0,0));
  CHECK(relevantWindowEvent(EVENT_SYSTEM_MINIMIZEEND,game,game,game));
  CHECK(relevantWindowEvent(EVENT_OBJECT_SHOW,replacement,replacement,game));
  CHECK(relevantWindowEvent(EVENT_OBJECT_LOCATIONCHANGE,game,game,game));
  CHECK(relevantWindowEvent(EVENT_OBJECT_DESTROY,game,replacement,game));
 });
 test("background windows and child controls do not trigger game discovery",[&]{
  CHECK(!discoveryEvent(EVENT_OBJECT_SHOW,OBJID_CLIENT,CHILDID_SELF));
  CHECK(!discoveryEvent(EVENT_OBJECT_SHOW,OBJID_WINDOW,1));
  CHECK(!discoveryEvent(EVENT_OBJECT_FOCUS,OBJID_WINDOW,0));
  CHECK(!relevantWindowEvent(EVENT_OBJECT_SHOW,other,game,game));
  CHECK(!relevantWindowEvent(EVENT_SYSTEM_MINIMIZEEND,other,game,game));
  CHECK(!relevantWindowEvent(EVENT_OBJECT_SHOW,nullptr,nullptr,nullptr));
 });
 test("transient missing foreground recovers on a delayed check",[]{
  FocusRetry retry;retry.observe(WindowState::Unavailable,1000);
  CHECK(retry.next()==1100);CHECK(!retry.takeDue(1099));CHECK(retry.takeDue(1100));
  retry.observe(WindowState::GameReady,1100);CHECK(retry.next()==0);CHECK(retry.attempts()==0);
 });
 test("retry burst stops within four seconds even under duplicate events",[]{
  FocusRetry retry;retry.observe(WindowState::GameEmpty,1000);
  unsigned checks=0;
  for(std::uint64_t now=1001;now<10000;++now){
   if(retry.takeDue(now)){++checks;retry.observe(WindowState::GameEmpty,now);}
   retry.observe(WindowState::GameEmpty,now);
  }
  CHECK(checks==6);CHECK(retry.next()==0);CHECK(retry.attempts()==6);
 });
 test("duplicate notifications do not postpone the first retry",[]{
  FocusRetry retry;retry.observe(WindowState::Unavailable,1000);
  for(std::uint64_t now=1001;now<1100;++now)retry.observe(WindowState::Unavailable,now);
  CHECK(retry.next()==1100);CHECK(retry.takeDue(1100));
 });
 test("switching away cancels retries and permits the next launch",[]{
  FocusRetry retry;retry.observe(WindowState::GameHidden,1000);retry.observe(WindowState::Other,1050);
  CHECK(retry.next()==0);CHECK(!retry.takeDue(1100));
  retry.observe(WindowState::GameHidden,2000);CHECK(retry.takeDue(2100));
  retry.observe(WindowState::Own,2100);CHECK(retry.next()==0);
 });
 test("late timer after sleep does not continue a stale recovery burst",[]{
  FocusRetry retry;retry.observe(WindowState::Unavailable,1000);
  CHECK(!retry.takeDue(30000));CHECK(retry.next()==0);
  retry.observe(WindowState::Unavailable,30000);CHECK(retry.next()==0);
 });
 test("window-show discovery still works after retry budget expires",[&]{
  FocusRetry retry;retry.observe(WindowState::GameHidden,1000);CHECK(!retry.takeDue(10000));
  CHECK(discoveryEvent(EVENT_OBJECT_SHOW,OBJID_WINDOW,CHILDID_SELF));
  CHECK(relevantWindowEvent(EVENT_OBJECT_SHOW,game,game,nullptr));
  retry.observe(gameWindowState(true,false,1920,1080),10000);CHECK(retry.next()==0);
  retry.observe(WindowState::Unavailable,11000);CHECK(retry.takeDue(11100));
 });
 test("real hidden helper is recognized as our process without taking focus",[]{
  auto foreground=GetForegroundWindow();
  auto window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,L"STATIC",L"Tracking test",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  CHECK(window);auto probe=probeWindow(window);DestroyWindow(window);
  CHECK(probe.window==window);CHECK(probe.pid==GetCurrentProcessId());CHECK(probe.state==WindowState::Own);
  CHECK(probeWindow(nullptr).state==WindowState::Unavailable);CHECK(GetForegroundWindow()==foreground);
 });
 test("hidden minimized and zero-size game windows are not ready",[]{
  CHECK(gameWindowState(false,false,1920,1080)==WindowState::GameHidden);
  CHECK(gameWindowState(true,true,1920,1080)==WindowState::GameMinimized);
  CHECK(gameWindowState(true,false,0,1080)==WindowState::GameEmpty);
  CHECK(gameWindowState(true,false,1920,0)==WindowState::GameEmpty);
  CHECK(gameWindowState(true,false,1920,1080)==WindowState::GameReady);
 });
 std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
