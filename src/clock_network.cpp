#include <winsock2.h>
#include <ws2tcpip.h>
#include "platform.h"
#ifdef __MINGW32__
// The bundled MinGW headers omit these declarations; its import library exports both.
extern "C" WINSOCK_API_LINKAGE int WSAAPI GetAddrInfoExCancel(LPHANDLE);
extern "C" WINSOCK_API_LINKAGE int WSAAPI GetAddrInfoExOverlappedResult(LPOVERLAPPED);
#endif

namespace sanctuary {
namespace {
struct Sockets {bool ready{};~Sockets(){if(ready)WSACleanup();}};
struct Addresses {ADDRINFOEXW* p{};~Addresses(){if(p)FreeAddrInfoExW(p);}};
struct Socket {SOCKET s{INVALID_SOCKET};~Socket(){if(s!=INVALID_SOCKET)closesocket(s);}};
struct Event {HANDLE h{CreateEventW(nullptr,TRUE,FALSE,nullptr)};~Event(){if(h)CloseHandle(h);}};
}
ClockResponse fetchClock(const std::atomic_bool* running){
 ClockResponse out;
 auto cancelled=[&]{return running&&!running->load();};
 auto fail=[&](const std::string& why){out.error=why;return out;};
 if(cancelled())return fail("cancelled");
 WSADATA data{};Sockets sockets;int code=WSAStartup(MAKEWORD(2,2),&data);
 if(code)return fail("NTP socket initialization: "+std::to_string(code));sockets.ready=true;
 ADDRINFOEXW hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_DGRAM;hints.ai_protocol=IPPROTO_UDP;
 Addresses addresses;timeval dnsTimeout{4,0};Event completed;
 if(!completed.h)return fail("NTP name lookup event failed");
 OVERLAPPED lookup{};lookup.hEvent=completed.h;HANDLE cancellation{};
 // Windows accepts the DNS timeout with an asynchronous lookup.
 code=GetAddrInfoExW(L"time.windows.com",L"123",NS_DNS,nullptr,&hints,&addresses.p,&dnsTimeout,&lookup,nullptr,&cancellation);
 if(code==WSA_IO_PENDING){
  auto started=GetTickCount64();
  while(WaitForSingleObject(completed.h,100)==WAIT_TIMEOUT){
   if(cancelled()||GetTickCount64()-started>=5000){GetAddrInfoExCancel(&cancellation);WaitForSingleObject(completed.h,INFINITE);break;}
  }
  code=GetAddrInfoExOverlappedResult(&lookup);
 }
 if(cancelled())return fail("cancelled");
 if(code)return fail("NTP name lookup: "+std::to_string(code));
 Socket peer;
 for(auto address=addresses.p;address;address=address->ai_next){
  auto socket=::socket(address->ai_family,SOCK_DGRAM,IPPROTO_UDP);if(socket==INVALID_SOCKET)continue;
  if(connect(socket,address->ai_addr,static_cast<int>(address->ai_addrlen))==0){peer.s=socket;break;}closesocket(socket);
 }
 if(peer.s==INVALID_SOCKET)return fail("NTP connection failed");
 if(cancelled())return fail("cancelled");
 auto sent=utcMillis();auto sentTicks=GetTickCount64();auto request=ntpRequest(sent);
 if(send(peer.s,reinterpret_cast<const char*>(request.data()),static_cast<int>(request.size()),0)!=static_cast<int>(request.size()))return fail("NTP send failed");
 // The connected UDP socket accepts replies only from the selected server/port.
 while(GetTickCount64()-sentTicks<2000){
  if(cancelled())return fail("cancelled");
  fd_set readers;FD_ZERO(&readers);FD_SET(peer.s,&readers);timeval wait{0,100000};
  int ready=select(0,&readers,nullptr,nullptr,&wait);
  if(ready==SOCKET_ERROR)return fail("NTP receive failed");if(!ready)continue;
  std::array<std::uint8_t,512> response{};int size=recv(peer.s,reinterpret_cast<char*>(response.data()),static_cast<int>(response.size()),0);
  auto received=utcMillis();auto receivedTicks=GetTickCount64();
  if(size<0)return fail("NTP receive failed");
  out.sample=parseNtpReply(std::span(response).first(static_cast<size_t>(size)),request,sent,received,sentTicks,receivedTicks);
  if(!out.sample)out.error="NTP reply failed validation";
  return out;
 }
 return fail("NTP response timed out");
}
}
