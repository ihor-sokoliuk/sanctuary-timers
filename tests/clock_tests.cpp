#include "clock.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace sanctuary;
int passed=0,failed=0;
#define CHECK(v) do{if(!(v))throw std::runtime_error(#v);}while(false)
void test(const char* name,std::function<void()> f){try{f();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
// Fixture encoding is independent of the production request encoder.
void stamp(NtpPacket& p,int index,Millis ms){
 auto seconds=static_cast<std::uint32_t>(ms/1000+2208988800LL);
 auto fraction=static_cast<std::uint32_t>(((static_cast<std::uint64_t>(ms%1000)<<32)+500)/1000);
 for(int i=0;i<4;++i){p[index+i]=static_cast<std::uint8_t>(seconds>>(24-8*i));p[index+4+i]=static_cast<std::uint8_t>(fraction>>(24-8*i));}
}
NtpPacket reply(const NtpPacket& request,Millis received,Millis sent){NtpPacket p{};p[0]=0x24;p[1]=2;std::copy_n(request.begin()+40,8,p.begin()+24);stamp(p,32,received);stamp(p,40,sent);return p;}
constexpr Millis wall=1790430000000;
int main(){
 test("request uses client mode and exact transmit timestamp",[]{auto p=ntpRequest(wall+125);NtpPacket expected{};expected[0]=0x23;stamp(expected,40,wall+125);CHECK(p==expected);});
 test("reply corrects a clock three seconds ahead with network delay",[]{auto q=ntpRequest(wall);auto p=reply(q,wall-2990,wall-2980);auto s=parseNtpReply(p,q,wall,wall+30,100,130);CHECK(s);CHECK(s->offsetMillis==-3000);CHECK(s->roundTripMillis==20);CHECK(s->utcMillis==wall-2970);CHECK(s->ticks==130);});
 test("reply corrects a clock two seconds behind",[]{auto q=ntpRequest(wall);auto s=parseNtpReply(reply(q,wall+2010,wall+2020),q,wall,wall+30,100,130);CHECK(s&&s->offsetMillis==2000);});
 test("reject malformed unsynchronized wrong mode and rate limit replies",[]{auto q=ntpRequest(wall);auto good=reply(q,wall+10,wall+20);CHECK(!parseNtpReply(std::span(good).first(47),q,wall,wall+30,100,130));for(auto byte:{0xe4,0x23,0x14}){auto p=good;p[0]=static_cast<std::uint8_t>(byte);CHECK(!parseNtpReply(p,q,wall,wall+30,100,130));}for(auto stratum:{0,16}){auto p=good;p[1]=static_cast<std::uint8_t>(stratum);CHECK(!parseNtpReply(p,q,wall,wall+30,100,130));}});
 test("reject replay zero and reversed server timestamps",[]{auto q=ntpRequest(wall);auto p=reply(q,wall+10,wall+20);p[24]^=1;CHECK(!parseNtpReply(p,q,wall,wall+30,100,130));p=reply(q,wall+10,wall+20);std::fill(p.begin()+40,p.end(),0);CHECK(!parseNtpReply(p,q,wall,wall+30,100,130));CHECK(!parseNtpReply(reply(q,wall+20,wall+10),q,wall,wall+30,100,130));});
 test("reject delayed implausible and clock stepped samples",[]{auto q=ntpRequest(wall);auto p=reply(q,wall+10,wall+20);CHECK(!parseNtpReply(p,q,wall,wall+3000,100,3100));CHECK(!parseNtpReply(p,q,wall,wall+1030,100,130));CHECK(!parseNtpReply(reply(q,wall+400010,wall+400020),q,wall,wall+30,100,130));});
 test("NTP timestamps unfold correctly after the 2036 rollover",[]{Millis future=2208988800000LL;auto q=ntpRequest(future);auto s=parseNtpReply(reply(q,future+10,future+20),q,future,future+30,100,130);CHECK(s&&s->utcMillis==future+30);});
 test("first sync once then exactly twenty four hours without duplicate requests",[]{DailyClock c;CHECK(c.takeDue(100));CHECK(!c.takeDue(100));c.finish(ClockSample{wall,130,0,20},130);CHECK(c.untilDue(130)==DailyClock::Interval);CHECK(!c.takeDue(130+DailyClock::Interval-1));CHECK(c.takeDue(130+DailyClock::Interval));CHECK(!c.takeDue(130+DailyClock::Interval));});
 test("corrected clock survives system adjustment and overnight sleep",[]{DailyClock c;CHECK(c.now(wall,100)==wall);c.takeDue(100);c.finish(ClockSample{wall-3000,130,-3000,20},130);CHECK(c.now(wall+600000,1130)==wall-2000);CHECK(c.now(wall,130+13*3600000)==wall-3000+13*3600000);CHECK(!c.takeDue(130+13*3600000));CHECK(c.takeDue(130+2*DailyClock::Interval));});
 test("failed daily check keeps last correction with bounded exponential retry",[]{DailyClock c;c.takeDue(0);c.finish(ClockSample{wall,0,0,0},0);c.takeDue(DailyClock::Interval);c.finish({},DailyClock::Interval);CHECK(c.failures()==1);CHECK(c.untilDue(DailyClock::Interval)==900000);CHECK(c.now(0,DailyClock::Interval)==wall+DailyClock::Interval);std::uint64_t t=DailyClock::Interval;for(int i=0;i<8;++i){t+=static_cast<std::uint64_t>(c.untilDue(t));CHECK(c.takeDue(t));c.finish({},t);}CHECK(c.untilDue(t)==21600000);c.takeDue(t+21600000);c.finish(ClockSample{wall,t+21600000,0,0},t+21600000);CHECK(c.failures()==0&&c.untilDue(t+21600000)==DailyClock::Interval);});
 test("settings distinguish pending failed and daily clock checks",[]{DailyClock c;CHECK(c.summary(0)==L"Clock: waiting for sync");c.takeDue(0);CHECK(c.summary(0)==L"Clock: syncing...");c.finish({},0);CHECK(c.summary(0)==L"Clock: Windows; retry pending");c.takeDue(900000);c.finish(ClockSample{wall,900000,0,0},900000);CHECK(c.summary(960000)==L"Clock: synced 1 min ago (daily)");});
 std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
