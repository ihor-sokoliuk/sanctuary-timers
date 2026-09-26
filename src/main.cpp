#include "platform.h"
#include "render.h"
#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>

using namespace sanctuary;
namespace {
constexpr wchar_t DisplayClass[]=L"SanctuaryTimersDisplay";
constexpr wchar_t InputClass[]=L"SanctuaryTimersControls";
constexpr UINT FocusMessage=WM_APP+1,DataMessage=WM_APP+2,StatusMessage=WM_APP+3,SnapshotMessage=WM_APP+4,TrayMessage=WM_APP+5,LocationMessage=WM_APP+6,ClockMessage=WM_APP+7;
constexpr UINT_PTR PaintTimer=1,NetworkTimer=2,ClockTimer=3,FocusTimer=4;
HWND appWindow{};
void CALLBACK windowEvent(HWINEVENTHOOK,DWORD event,HWND window,LONG object,LONG child,DWORD,DWORD){
 if(!appWindow)return;
 if(discoveryEvent(event,object,child)){
  if(event==EVENT_OBJECT_SHOW&&GetAncestor(window,GA_ROOT)!=window)return;
  PostMessageW(appWindow,FocusMessage,event,reinterpret_cast<LPARAM>(window));
 }
 else if(object==OBJID_WINDOW&&child==CHILDID_SELF)PostMessageW(appWindow,LocationMessage,event,reinterpret_cast<LPARAM>(window));
}
struct Batch {int requested{},performed{};std::array<Response,3> response;};
class App {
 public:
 HWND display{},input{},game{};
 Preferences prefs;Coordinator coordinator;Renderer renderer;DailyClock clock;
 std::filesystem::path directory;
 HWINEVENTHOOK focusHook{},showHook{},restoreHook{},locationHook{},destroyHook{};
 DWORD gamePid{},hookedPid{};float scale{1};Box bounds{};
 FocusRetry focusRetry;WindowProbe lastProbe;bool haveProbe{};unsigned recoveryChecks{};
 bool visible{},settings{},editing{},dragging{},smoke{},writable{},startupOk{},closing{};
 POINT dragStart{};int dragX{},dragY{};
 std::atomic_bool active{false};std::thread worker;
 std::atomic_bool running{true};std::thread clockWorker;unsigned clockReads{};
 NOTIFYICONDATAW tray{};HICON trayIcon{};
 unsigned long long paints{},paintErrors{},focusChanges{};std::array<unsigned,3> reads{},failures{};
 std::wstring status=L"Waiting for Diablo IV";
 explicit App(bool test):smoke(test){directory=executablePath().parent_path();prefs=smoke?Preferences{}:loadPreferences(directory/L"settings.ini");if(!smoke)coordinator.records=loadCache(directory/L"events.ini",utcNow());}
 ~App(){active=false;running=false;if(worker.joinable())worker.join();if(clockWorker.joinable())clockWorker.join();if(focusHook)UnhookWinEvent(focusHook);if(showHook)UnhookWinEvent(showHook);if(restoreHook)UnhookWinEvent(restoreHook);if(locationHook)UnhookWinEvent(locationHook);if(destroyHook)UnhookWinEvent(destroyHook);if(tray.hWnd)Shell_NotifyIconW(NIM_DELETE,&tray);if(trayIcon)DestroyIcon(trayIcon);}
 Millis nowMillis()const{return clock.now(utcMillis(),GetTickCount64());}
 Time now()const{return nowMillis()/1000;}
 void log(const std::string& message){if(smoke)return;auto path=directory/L"diagnostics.log";std::error_code ec;if(std::filesystem::file_size(path,ec)>256*1024&&!ec){std::ofstream clear(path,std::ios::trunc);}std::ofstream f(path,std::ios::app);f<<utcNow()<<' '<<message<<'\n';}
 void save(){if(!smoke&&!savePreferences(directory/L"settings.ini",prefs)){status=L"Cannot save preferences";log("preferences write failed");}}
 void dump(){
  std::ofstream f(directory/L"status.txt");auto foreground=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(foreground,&pid);
  f<<"utc="<<utcNow()<<"\nvisible="<<visible<<"\nforeground_pid="<<pid<<"\ngame_pid="<<gamePid<<"\nsettings="<<settings<<"\ncollapsed="<<prefs.collapsed<<"\nstartup_registered="<<prefs.registered<<"\nstartup_ok="<<startupOk<<"\npaints="<<paints<<"\npaint_errors="<<paintErrors<<"\nfocus_changes="<<focusChanges<<"\nscale="<<scale<<"\nx="<<bounds.x<<"\ny="<<bounds.y<<"\nwidth="<<bounds.width<<"\nheight="<<bounds.height<<"\n";
  f<<"clock_utc_ms="<<nowMillis()<<"\nclock_reads="<<clockReads<<"\nclock_failures="<<clock.failures()<<"\nclock_synced="<<bool(clock.sample())<<"\nclock_next_ms="<<clock.untilDue(GetTickCount64())<<'\n';
  f<<"window_state="<<windowStateName(lastProbe.state)<<"\nwindow_error="<<lastProbe.error<<"\ndiscovery_hooks_ok="<<bool(focusHook&&showHook&&restoreHook)<<"\nfocus_retry_checks="<<recoveryChecks<<"\nfocus_retry_pending="<<bool(focusRetry.next())<<'\n';
  if(clock.sample())f<<"clock_checked="<<clock.sample()->utcMillis/1000<<"\nclock_offset_ms="<<clock.sample()->offsetMillis<<"\nclock_roundtrip_ms="<<clock.sample()->roundTripMillis<<'\n';
  for(int i=0;i<3;++i)f<<"event"<<i<<"_reads="<<reads[i]<<"\nevent"<<i<<"_failures="<<failures[i]<<"\nevent"<<i<<"_checked="<<coordinator.records[i].checked<<"\nevent"<<i<<"_verified="<<coordinator.records[i].verified<<"\nevent"<<i<<"_due="<<coordinator.requests[i].due<<'\n';
 }
 HICON makeIcon(){
  HDC dc=CreateCompatibleDC(nullptr);BITMAPV5HEADER info{};info.bV5Size=sizeof(info);info.bV5Width=32;info.bV5Height=-32;info.bV5Planes=1;info.bV5BitCount=32;info.bV5Compression=BI_BITFIELDS;info.bV5RedMask=0x00ff0000;info.bV5GreenMask=0x0000ff00;info.bV5BlueMask=0x000000ff;info.bV5AlphaMask=0xff000000;
  void* data{};auto bitmap=CreateDIBSection(dc,reinterpret_cast<BITMAPINFO*>(&info),DIB_RGB_COLORS,&data,nullptr,0);
  if(!bitmap){DeleteDC(dc);return CopyIcon(LoadIconW(nullptr,IDI_APPLICATION));}
  auto pixels=static_cast<unsigned*>(data);for(int y=0;y<32;++y)for(int x=0;x<32;++x){bool outside=(x<2||x>29||y<2||y>29);bool fire=y>5&&y<27&&abs(x-16)<std::min(8,(y-4)/2)&&!(y<14&&x>17);pixels[y*32+x]=outside?0:fire?0xffffb566:0xff151923;}
  HBITMAP mask=CreateBitmap(32,32,1,1,nullptr);ICONINFO ii{};ii.fIcon=TRUE;ii.hbmColor=bitmap;ii.hbmMask=mask;auto icon=CreateIconIndirect(&ii);DeleteObject(bitmap);DeleteObject(mask);DeleteDC(dc);return icon;
 }
 bool init(){
  WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=procedure;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.lpszClassName=DisplayClass;RegisterClassExW(&cls);cls.lpszClassName=InputClass;cls.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));RegisterClassExW(&cls);
  display=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST,DisplayClass,L"Sanctuary Timers",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,this);
  if(!display)return false;
  input=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED|WS_EX_TOPMOST,InputClass,L"Sanctuary Timers controls",WS_POPUP,0,0,1,1,display,nullptr,cls.hInstance,this);
  if(!input||!SetLayeredWindowAttributes(input,0,1,LWA_ALPHA)||!renderer.init())return false;
  appWindow=display;
  if(smoke)return true;
  writable=savePreferences(directory/L"settings.ini",prefs);
  if(writable){startupOk=registerStartup(executablePath(),prefs);save();}else log("portable folder is not writable; startup not registered");
  trayIcon=static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,0));if(!trayIcon)trayIcon=makeIcon();tray.cbSize=sizeof(tray);tray.hWnd=display;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=TrayMessage;tray.hIcon=trayIcon;wcscpy_s(tray.szTip,L"Sanctuary Timers");Shell_NotifyIconW(NIM_ADD,&tray);
  focusHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,windowEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
  showHook=SetWinEventHook(EVENT_OBJECT_SHOW,EVENT_OBJECT_SHOW,nullptr,windowEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
  restoreHook=SetWinEventHook(EVENT_SYSTEM_MINIMIZEEND,EVENT_SYSTEM_MINIMIZEEND,nullptr,windowEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
  if(!focusHook||!showHook||!restoreHook){log("window discovery hook unavailable");return false;}
  log("started; no-activate display and event-driven tracking");focus("startup");scheduleClock();return true;
 }
 void hooks(DWORD pid){
  if(locationHook)UnhookWinEvent(locationHook);if(destroyHook)UnhookWinEvent(destroyHook);
  locationHook=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,windowEvent,pid,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
  destroyHook=SetWinEventHook(EVENT_OBJECT_DESTROY,EVENT_OBJECT_DESTROY,nullptr,windowEvent,pid,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
  hookedPid=pid;if(!locationHook||!destroyHook)log("game location/destroy hook unavailable");
 }
 void hide(){visible=false;active=false;dragging=false;KillTimer(display,PaintTimer);KillTimer(display,NetworkTimer);ShowWindow(input,SW_HIDE);ShowWindow(display,SW_HIDE);if(GetCapture()==input)ReleaseCapture();}
 void focus(const char* source,bool geometry=false){
  ++focusChanges;
  // Verify the current foreground window, rather than trusting a queued historical event.
  auto probe=probeWindow(GetForegroundWindow());auto tick=GetTickCount64();focusRetry.observe(probe.state,tick);
  KillTimer(display,FocusTimer);
  if(focusRetry.next())SetTimer(display,FocusTimer,static_cast<UINT>(focusRetry.next()>tick?focusRetry.next()-tick:1),nullptr);
  if(!haveProbe||probe.window!=lastProbe.window||probe.pid!=lastProbe.pid||probe.state!=lastProbe.state||probe.error!=lastProbe.error){
   log(std::string("window check source=")+source+" hwnd="+std::to_string(reinterpret_cast<std::uintptr_t>(probe.window))+" pid="+std::to_string(probe.pid)+" state="+windowStateName(probe.state)+" error="+std::to_string(probe.error));
   lastProbe=probe;haveProbe=true;
  }
  if(probe.state==WindowState::Own)return;
  bool gameWindow=probe.state==WindowState::GameReady||probe.state==WindowState::GameHidden||probe.state==WindowState::GameMinimized||probe.state==WindowState::GameEmpty;
  bool changedWindow=game!=probe.window;
  if(gameWindow){game=probe.window;if(hookedPid!=probe.pid)hooks(probe.pid);}
  if(probe.state==WindowState::GameReady){
   bool newSession=probe.pid!=gamePid;
   if(visible&&!editing&&!newSession&&!changedWindow){if(geometry){position();paint();}return;}
   gamePid=probe.pid;editing=false;active=true;visible=true;coordinator.activate(now(),newSession);position();paint();ShowWindow(display,SW_SHOWNOACTIVATE);ShowWindow(input,SW_SHOWNOACTIVATE);schedule();log(newSession?"game session active":"game foreground");
  }else {editing=false;hide();}
 }
 void position(){
  Box area;
  if(game&&IsWindow(game)&&!IsIconic(game)){area=gameBounds(game);scale=GetDpiForWindow(game)/96.f;}
  else {RECT r{};SystemParametersInfoW(SPI_GETWORKAREA,0,&r,0);area={r.left,r.top,r.right-r.left,r.bottom-r.top};scale=GetDpiForWindow(display)/96.f;}
  scale=std::clamp(scale,1.f,4.f);auto l=layout(prefs,settings);bounds=place(area,static_cast<int>(l.width*scale),static_cast<int>(l.height*scale),static_cast<int>(prefs.x*scale),static_cast<int>(prefs.y*scale));
  int inputWidth=settings?bounds.width:std::min(bounds.width,static_cast<int>(28*scale));
  SetWindowPos(display,HWND_TOPMOST,bounds.x,bounds.y,bounds.width,bounds.height,SWP_NOACTIVATE);
  SetWindowPos(input,HWND_TOPMOST,bounds.x,bounds.y,inputWidth,bounds.height,SWP_NOACTIVATE);
 }
 void paint(){if(!visible&&!smoke)return;++paints;if(!renderer.draw(display,prefs,settings,coordinator.records,now(),scale,bounds,status,clock.summary(GetTickCount64()))){++paintErrors;if(paintErrors<5)log("layered drawing failed: "+std::to_string(GetLastError()));}}
 void armPaint(){if(visible&&(settings||!prefs.collapsed))SetTimer(display,PaintTimer,static_cast<UINT>(std::clamp<Millis>(1000-nowMillis()%1000,15,1000)),nullptr);}
 void scheduleClock(){
  KillTimer(display,ClockTimer);if(smoke||!running)return;
  if(!clockWorker.joinable()&&clock.takeDue(GetTickCount64())){
   clockWorker=std::thread([this]{auto result=std::make_unique<ClockResponse>(fetchClock(&running));if(PostMessageW(display,ClockMessage,0,reinterpret_cast<LPARAM>(result.get())))result.release();});
  }
  SetTimer(display,ClockTimer,static_cast<UINT>(std::clamp<Millis>(clock.untilDue(GetTickCount64()),1,2147483000)),nullptr);
 }
 void clockData(ClockResponse* raw){
  std::unique_ptr<ClockResponse> response(raw);if(clockWorker.joinable())clockWorker.join();++clockReads;
  clock.finish(response->sample,GetTickCount64());
  if(response->sample)log("clock synced source=time.windows.com offset_ms="+std::to_string(response->sample->offsetMillis)+" roundtrip_ms="+std::to_string(response->sample->roundTripMillis)+" next_check_seconds=86400");
  else log("clock sync failed: "+response->error);
  scheduleClock();schedule();paint();dump();
 }
 void schedule(){
  coordinator.observeClock(now(),GetTickCount64());
  KillTimer(display,PaintTimer);KillTimer(display,NetworkTimer);
  armPaint();
  if(!active)return;
  auto now=this->now();coordinator.reconcile(now);auto due=coordinator.nextWake(now);
  if(due!=Never)SetTimer(display,NetworkTimer,static_cast<UINT>(std::clamp<Time>((due-now)*1000,1,2147483000)),nullptr);
  startRequests(now);
 }
 void startRequests(Time now){
  if(worker.joinable())return;
  int mask=coordinator.takeDue(now,active);if(!mask)return;
  status=L"Checking event times…";
  auto eventClock=clock;
  worker=std::thread([this,mask,eventClock]{
   auto result=std::make_unique<Batch>();result->requested=mask;
   for(int i=0;i<3;++i)if(mask&(1<<i)){if(!active)break;result->performed|=1<<i;result->response[i]=fetchRecord(Kind(i),eventClock.now(utcMillis(),GetTickCount64())/1000,&active);}
   if(PostMessageW(display,DataMessage,0,reinterpret_cast<LPARAM>(result.get())))result.release();
  });
 }
 void data(Batch* raw){
  std::unique_ptr<Batch> result(raw);if(worker.joinable())worker.join();bool all=true;auto now=this->now();
  for(int i=0;i<3;++i)if(result->requested&(1<<i)){
   if(!(result->performed&(1<<i))||result->response[i].error=="cancelled"){coordinator.requests[i].busy=false;continue;}
   ++reads[i];auto& response=result->response[i];bool ok=response.record&&acceptRecord(Kind(i),coordinator.records[i],*response.record);
   coordinator.complete(Kind(i),now,ok,response.retryAfter);if(!ok){++failures[i];all=false;}
   log("event="+std::to_string(i)+" status="+std::to_string(response.status)+" accepted="+(ok?"yes":"no")+(response.error.empty()?"":" "+response.error));
  }
  status=all?L"Timers calculated locally":L"Using cached estimates; retry pending";
  if(!writable)status=L"Cannot save: folder is read-only";
  else if(!startupOk)status=L"Startup registration failed";
  if(writable&&!saveCache(directory/L"events.ini",coordinator.records))log("cache write failed");
  paint();dump();schedule();
 }
 void click(int x,int y){
  auto hit=hitTest(prefs,settings,x,y);
  switch(hit){
   case Hit::Collapse:prefs.collapsed=!prefs.collapsed;break;
   case Hit::Settings:settings=true;break;
   case Hit::Back:settings=false;break;
   case Hit::FontDown:--prefs.font;break;case Hit::FontUp:++prefs.font;break;
   case Hit::WidthDown:prefs.width-=5;break;case Hit::WidthUp:prefs.width+=5;break;
   case Hit::OpacityDown:prefs.opacity-=5;break;case Hit::OpacityUp:prefs.opacity+=5;break;
   case Hit::Drag:dragging=true;GetCursorPos(&dragStart);dragX=prefs.x;dragY=prefs.y;SetCapture(input);return;
   default:return;
  }
  prefs=normalize(prefs);save();position();paint();schedule();
 }
 void trayMenu(){
  auto menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Appearance");AppendMenuW(menu,MF_STRING,2,L"Exit");POINT cursor{};GetCursorPos(&cursor);
  // Only a user's tray click reaches this path; automatic actions never activate windows.
  SetForegroundWindow(input);auto chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,cursor.x,cursor.y,0,input,nullptr);DestroyMenu(menu);
  if(chosen==2)PostMessageW(display,WM_CLOSE,0,0);
  if(chosen==1){settings=true;editing=true;visible=true;position();paint();ShowWindow(display,SW_SHOWNOACTIVATE);ShowWindow(input,SW_SHOWNOACTIVATE);schedule();}
 }
 static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l){
  App* a=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
  if(message==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}
  if(!a)return DefWindowProcW(hwnd,message,w,l);
  switch(message){
   case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
   case WM_ERASEBKGND:return 1;
   case WM_PAINT: {PAINTSTRUCT ps{};auto dc=BeginPaint(hwnd,&ps);if(hwnd==a->input)FillRect(dc,&ps.rcPaint,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));EndPaint(hwnd,&ps);return 0;}
   case FocusMessage:if(relevantWindowEvent(static_cast<DWORD>(w),reinterpret_cast<HWND>(l),GetForegroundWindow(),a->game))a->focus(w==EVENT_OBJECT_SHOW?"show":w==EVENT_SYSTEM_MINIMIZEEND?"restore":"foreground");return 0;
   case LocationMessage:if(relevantWindowEvent(static_cast<DWORD>(w),reinterpret_cast<HWND>(l),GetForegroundWindow(),a->game))a->focus(w==EVENT_OBJECT_DESTROY?"destroy":"location",true);return 0;
   case DataMessage:a->data(reinterpret_cast<Batch*>(l));return 0;
   case ClockMessage:a->clockData(reinterpret_cast<ClockResponse*>(l));return 0;
   case StatusMessage:a->dump();return 0;
   case SnapshotMessage:a->renderer.saveBitmap((a->directory/L"preview.bmp").wstring());return 0;
   case WM_TIMER:
    if(w==PaintTimer){KillTimer(hwnd,PaintTimer);if(a->coordinator.observeClock(a->now(),GetTickCount64()))a->schedule();a->paint();a->armPaint();}
    else if(w==NetworkTimer){KillTimer(hwnd,NetworkTimer);a->schedule();}
    else if(w==ClockTimer)a->scheduleClock();
    else if(w==FocusTimer){KillTimer(hwnd,FocusTimer);if(a->focusRetry.takeDue(GetTickCount64())){++a->recoveryChecks;a->focus("retry");}}return 0;
   case WM_TIMECHANGE:case WM_POWERBROADCAST:if(message==WM_TIMECHANGE||w==PBT_APMRESUMEAUTOMATIC||w==PBT_APMRESUMESUSPEND){a->coordinator.observeClock(a->now(),GetTickCount64());a->coordinator.activate(a->now(),true);a->focusRetry={};a->focus("resume/time-change");a->schedule();a->scheduleClock();}return TRUE;
   case WM_DPICHANGED:case WM_DISPLAYCHANGE:if(a->visible){a->position();a->paint();}return 0;
   case WM_LBUTTONDOWN:if(hwnd==a->input)a->click(static_cast<int>(GET_X_LPARAM(l)/a->scale),static_cast<int>(GET_Y_LPARAM(l)/a->scale));return 0;
   case WM_MOUSEMOVE:if(a->dragging){POINT p{};GetCursorPos(&p);a->prefs.x=std::max(0,a->dragX+static_cast<int>((p.x-a->dragStart.x)/a->scale));a->prefs.y=std::max(0,a->dragY+static_cast<int>((p.y-a->dragStart.y)/a->scale));a->position();a->paint();}return 0;
   case WM_LBUTTONUP:if(a->dragging){a->dragging=false;ReleaseCapture();a->save();}return 0;
   case WM_CAPTURECHANGED:a->dragging=false;return 0;
   case WM_SETCURSOR:if(hwnd==a->input){POINT p{};GetCursorPos(&p);ScreenToClient(hwnd,&p);auto hit=hitTest(a->prefs,a->settings,static_cast<int>(p.x/a->scale),static_cast<int>(p.y/a->scale));SetCursor(LoadCursorW(nullptr,hit==Hit::Drag?IDC_SIZEALL:IDC_HAND));return TRUE;}break;
   case TrayMessage:if(l==WM_RBUTTONUP||l==WM_LBUTTONUP)a->trayMenu();return 0;
   case WM_CLOSE:if(hwnd==a->display&&!a->closing){a->closing=true;a->hide();PostQuitMessage(0);}return 0;
   case WM_QUERYENDSESSION:return TRUE;
   case WM_ENDSESSION:if(w){a->hide();PostQuitMessage(0);}return 0;
   default:break;
  }
  return DefWindowProcW(hwnd,message,w,l);
 }
};
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR args,int){
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 auto command=std::wstring(args?args:L"");
 if(command==L"--quit"||command==L"--status"||command==L"--snapshot"){
  auto existing=FindWindowW(DisplayClass,nullptr);if(!existing)return 2;
  return PostMessageW(existing,command==L"--quit"?WM_CLOSE:command==L"--status"?StatusMessage:SnapshotMessage,0,0)?0:3;
 }
 bool smoke=command==L"--smoke";
 if(!smoke&&!command.empty())return 2;
 HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\SanctuaryTimers.Singleton");if(!mutex)return 3;if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(mutex);return 0;}
 CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 int result=0;
 {
  App app(smoke);
  if(!app.init()){app.log("initialization failed");result=4;}
  else if(smoke){
   auto foreground=GetForegroundWindow();app.bounds={24,24,270,83};app.visible=false;
   app.coordinator.records[0]={utcNow()+35*60,utcNow()+35*60+12600,0,utcNow(),true,false};app.coordinator.records[2]={utcNow()+7*60,utcNow()+7*60+1500,0,utcNow(),true,false};app.coordinator.records[1].checked=utcNow();
   app.paint();bool styles=(GetWindowLongPtrW(app.display,GWL_EXSTYLE)&WS_EX_TRANSPARENT)&&(GetWindowLongPtrW(app.input,GWL_EXSTYLE)&WS_EX_NOACTIVATE);
   bool image=app.renderer.saveBitmap((app.directory/L"preview.bmp").wstring());
   result=app.paintErrors||GetForegroundWindow()!=foreground||IsWindowVisible(app.display)||!styles||!image?5:0;
   std::ofstream(app.directory/L"smoke-result.txt")<<"exit="<<result<<" paints="<<app.paints<<" no_focus_change="<<(GetForegroundWindow()==foreground)<<" hidden="<<!IsWindowVisible(app.display)<<" styles="<<styles<<" image="<<image<<'\n';
  }else{
   MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
  }
  app.active=false;app.running=false;if(app.worker.joinable())app.worker.join();if(app.clockWorker.joinable())app.clockWorker.join();
  MSG pending{};while(PeekMessageW(&pending,app.display,DataMessage,DataMessage,PM_REMOVE))delete reinterpret_cast<Batch*>(pending.lParam);
  while(PeekMessageW(&pending,app.display,ClockMessage,ClockMessage,PM_REMOVE))delete reinterpret_cast<ClockResponse*>(pending.lParam);
  appWindow=nullptr;if(app.input)DestroyWindow(app.input);if(app.display)DestroyWindow(app.display);
 }
 CoUninitialize();CloseHandle(mutex);return result;
}
