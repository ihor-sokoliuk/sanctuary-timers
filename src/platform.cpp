#include "platform.h"
#include <winhttp.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cwctype>
#include <fstream>
#include <sstream>
namespace sanctuary {
namespace {
Time number(const std::filesystem::path& p,const wchar_t* section,const wchar_t* key,Time fallback=0) {
 wchar_t text[80]{};GetPrivateProfileStringW(section,key,L"",text,80,p.c_str());
 wchar_t* end=nullptr;errno=0;auto n=wcstoll(text,&end,10);while(end&&iswspace(*end))++end;
 return errno||end==text||!end||*end?fallback:n;
}
bool atomicWrite(const std::filesystem::path& path,const std::string& text) {
 try {
  auto tmp=path;tmp+=L".tmp";
  {std::ofstream f(tmp,std::ios::binary|std::ios::trunc);f<<text;f.flush();if(!f)return false;}
  return MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
 }catch(...){return false;}
}
struct Internet {
 HINTERNET h{};
 explicit Internet(HINTERNET value):h(value){}
 ~Internet(){if(h)WinHttpCloseHandle(h);}
 Internet(const Internet&)=delete;Internet& operator=(const Internet&)=delete;
 operator HINTERNET()const{return h;}
};
}
Millis utcMillis(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
Time utcNow(){return utcMillis()/1000;}
Preferences loadPreferences(const std::filesystem::path& path){
 Preferences p;
 auto read=[&](const wchar_t* key,int fallback){return static_cast<int>(std::clamp<Time>(number(path,L"appearance",key,fallback),-100000,100000));};
 p.font=read(L"font",13);p.width=read(L"width",270);p.opacity=read(L"opacity",85);p.x=read(L"x",24);p.y=read(L"y",24);
 p.collapsed=number(path,L"appearance",L"collapsed")==1;p.registered=number(path,L"system",L"startup_registered")==1;
 return normalize(p);
}
bool savePreferences(const std::filesystem::path& path,Preferences p){
 p=normalize(p);std::ostringstream s;
 s<<"[appearance]\nfont="<<p.font<<"\nwidth="<<p.width<<"\nopacity="<<p.opacity<<"\nx="<<p.x<<"\ny="<<p.y<<"\ncollapsed="<<p.collapsed<<"\n[system]\nstartup_registered="<<p.registered<<'\n';
 return atomicWrite(path,s.str());
}
std::array<Record,3> loadCache(const std::filesystem::path& path,Time now){
 std::array<Record,3> result{};
 for(int i=0;i<3;++i){auto section=std::to_wstring(i);auto n=[&](const wchar_t* key){return number(path,section.c_str(),key);};
  Record r{n(L"start"),n(L"next"),n(L"end"),n(L"checked"),n(L"verified")==1,true};
  bool valid=r.start>=1577836800&&r.start<=now+2*period(Kind(i))&&r.checked>=1577836800&&r.checked<=now+300;
  if(i==1)valid=valid&&r.end-r.start==3300&&r.start%3600==0;
  else valid=valid&&r.next>r.start&&r.next-r.start<=86400&&(!r.verified||r.next-r.start==period(Kind(i)));
  if(valid)result[i]=r;
 }return result;
}
bool saveCache(const std::filesystem::path& path,const std::array<Record,3>& records){
 std::ostringstream s;for(int i=0;i<3;++i){const auto& r=records[i];s<<'['<<i<<"]\nstart="<<r.start<<"\nnext="<<r.next<<"\nend="<<r.end<<"\nchecked="<<r.checked<<"\nverified="<<r.verified<<'\n';}return atomicWrite(path,s.str());
}
Response fetchRecord(Kind kind,Time now,const std::atomic_bool* allowed){
 Response out;auto fail=[&](const char* what){out.error=std::string(what)+" ("+std::to_string(GetLastError())+")";return out;};
 RequestBudget budget{GetTickCount64()};auto stop=[&]{bool running=!allowed||allowed->load();if(!budget.permits(GetTickCount64(),running)){out.error=running?"request deadline exceeded":"cancelled";return true;}return false;};
 if(stop())return out;
 Internet session(WinHttpOpen(L"SanctuaryTimers/0.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));if(!session.h)return fail("open");
 if(!WinHttpSetTimeouts(session,4000,4000,4000,4000))return fail("timeouts");if(stop())return out;
 Internet connection(WinHttpConnect(session,L"helltides-7e530-r1.firebaseio.com",INTERNET_DEFAULT_HTTPS_PORT,0));if(!connection.h)return fail("connect");
 const wchar_t* paths[]={L"/world_boss.json",L"/helltide.json",L"/legion.json"};
 Internet request(WinHttpOpenRequest(connection,L"GET",paths[static_cast<int>(kind)],nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));if(!request.h)return fail("request");
 DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
 if(stop())return out;
 if(!WinHttpSendRequest(request,L"Accept: application/json\r\n",DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0))return fail("send");
 if(stop())return out;
 if(!WinHttpReceiveResponse(request,nullptr))return fail("receive");if(stop())return out;
 DWORD status=0,size=sizeof(status);if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))return fail("status");out.status=status;
 wchar_t retry[128]{};size=sizeof(retry);
 if(WinHttpQueryHeaders(request,WINHTTP_QUERY_CUSTOM,L"Retry-After",retry,&size,WINHTTP_NO_HEADER_INDEX)){
  wchar_t* end=nullptr;auto seconds=wcstoll(retry,&end,10);
  if(end!=retry&&!*end&&seconds>0)out.retryAfter=std::min<Time>(seconds,7*86400);
  else {SYSTEMTIME st{};FILETIME ft{};if(WinHttpTimeToSystemTime(retry,&st)&&SystemTimeToFileTime(&st,&ft)){ULARGE_INTEGER n;n.LowPart=ft.dwLowDateTime;n.HighPart=ft.dwHighDateTime;out.retryAfter=std::clamp<Time>(Time(n.QuadPart/10000000ULL)-11644473600LL-now,0,7*86400);}}
 }
 if(status!=200){out.error="HTTP "+std::to_string(status);return out;}
 std::string body;char buffer[4096];DWORD got=0;
 do {if(stop())return out;if(!WinHttpReadData(request,buffer,sizeof(buffer),&got))return fail("body");if(body.size()+got>65536){out.error="oversized record";return out;}body.append(buffer,got);}while(got);
 out.record=parseRecord(kind,body,now);if(!out.record)out.error="invalid or stale record";return out;
}
bool registerStartup(const std::filesystem::path& exe,Preferences& p){
 constexpr auto key=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
 HKEY handle{};if(RegCreateKeyExW(HKEY_CURRENT_USER,key,0,nullptr,0,KEY_QUERY_VALUE|KEY_SET_VALUE,nullptr,&handle,nullptr)!=ERROR_SUCCESS)return false;
 wchar_t value[32768]{};DWORD bytes=sizeof(value),type{};auto rc=RegQueryValueExW(handle,L"Sanctuary Timers",nullptr,&type,reinterpret_cast<BYTE*>(value),&bytes);
 bool exists=rc==ERROR_SUCCESS;std::wstring desired=L"\""+exe.wstring()+L"\"";
 bool same=exists&&type==REG_SZ&&desired==value;
 auto action=startupAction(p.registered,exists,same);bool ok=true;
 if(action!=StartupAction::None)ok=RegSetValueExW(handle,L"Sanctuary Timers",0,REG_SZ,reinterpret_cast<const BYTE*>(desired.c_str()),static_cast<DWORD>((desired.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS;
 RegCloseKey(handle);if(ok)p.registered=true;return ok;
}
std::filesystem::path executablePath(){std::wstring path(32768,L'\0');auto n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));path.resize(n);return path;}
bool isGameWindow(HWND window){
 return probeWindow(window).state==WindowState::GameReady;
}
Box gameBounds(HWND window){RECT r{};if(!GetClientRect(window,&r))return {};POINT p{0,0};ClientToScreen(window,&p);return {p.x,p.y,r.right-r.left,r.bottom-r.top};}
}
