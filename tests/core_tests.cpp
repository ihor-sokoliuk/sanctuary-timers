#include "core.h"
#include <iostream>
#include <functional>
#include <stdexcept>
using namespace sanctuary;
int passed=0,failed=0;
void check(bool value,const char* expr) { if(!value) throw std::runtime_error(expr); }
#define CHECK(x) check(!!(x),#x)
void test(const char* name,std::function<void()> f) { try { f(); ++passed; std::cout<<"PASS "<<name<<'\n'; } catch(const std::exception& e) {++failed; std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';} }
int main() {
 test("UTC epoch and fractional feed timestamp",[]{CHECK(parseUtc("1970-01-01T00:00:00Z")==0); CHECK(parseUtc("2026-09-25T21:00:00.000Z")==1790370000);});
 test("invalid dates and timezone ambiguity rejected",[]{for(auto s:{"2026-02-29T12:00:00Z","2026-09-25T25:00:00Z","2026-09-25T21:00:00+03:00","junk","2026-09-25T21:00:00Zjunk"}) CHECK(!parseUtc(s));});
 test("Helltide start end break boundaries",[]{Record r{0,0,0,1,true,false}; CHECK(calculate(Kind::Helltide,r,3600).seconds==3300); CHECK(calculate(Kind::Helltide,r,6899).seconds==1); auto v=calculate(Kind::Helltide,r,6900);CHECK(!v.active&&v.seconds==300&&v.boundary==7200);});
 test("unknown Helltide still calculates estimated",[]{auto v=calculate(Kind::Helltide,{},3610); CHECK(v.available&&v.estimated&&v.active&&v.seconds==3290);});
 test("unknown boss has no invented timestamp",[]{auto v=calculate(Kind::Boss,{},10000);CHECK(!v.available);CHECK(formatTime(v)==L"—");});
 test("spawn Now lasts exactly sixty seconds",[]{Record r{10000,22600,0,9000,true,false}; CHECK(calculate(Kind::Boss,r,9999).seconds==1); CHECK(calculate(Kind::Boss,r,10000).now);CHECK(calculate(Kind::Boss,r,10059).now);CHECK(!calculate(Kind::Boss,r,10060).now);CHECK(calculate(Kind::Boss,r,10060).seconds==12540);});
 test("recurrence survives days and clock rewind",[]{Record r{10000,11500,0,9000,true,false};CHECK(calculate(Kind::Legion,r,100000).now);CHECK(calculate(Kind::Legion,r,99999).seconds==1); CHECK(calculate(Kind::Legion,r,5000).seconds==500);});
 test("failed cache retains estimates",[]{Record r{10000,22600,0,9000,true,true};auto v=calculate(Kind::Boss,r,10100);CHECK(v.available&&v.estimated&&v.seconds==12500);CHECK(formatTime(v)==L"~3:28:20");});
 test("source record parses ignored nested metadata",[]{auto r=parseRecord(Kind::Boss,R"({"boss":"x","unused":{"startTime":"bad"},"startTime":"2026-09-25T21:00:00.000Z","nextTime":"2026-09-26T00:30:00.000Z"})",1790369000);CHECK(r&&r->start==1790370000&&r->verified);});
 test("malformed missing duplicate and future fields rejected",[]{for(auto s:{R"({"startTime":2})",R"({"startTime":"2026-09-25T21:00:00Z","startTime":"2026-09-25T22:00:00Z"})",R"({"startTime":"2036-09-25T21:00:00Z"})","<html>blocked</html>","{}","null","{broken}"}) CHECK(!parseRecord(Kind::Boss,s,1790369000));});
 test("Helltide response validates fifty five minute duration",[]{CHECK(parseRecord(Kind::Helltide,R"({"startTime":"2026-09-25T20:00:00Z","endTime":"2026-09-25T20:55:00Z","nextTime":null})",1790369000));CHECK(!parseRecord(Kind::Helltide,R"({"startTime":"2026-09-25T20:00:00Z","endTime":"2026-09-25T23:55:00Z"})",1790369000));});
 test("older feed cannot roll back valid anchor",[]{Record a{10000,22600,0,9000,true,false};CHECK(!acceptRecord(Kind::Boss,a,{9000,21600,0,10000,true,false}));CHECK(a.start==10000);});
 test("phase change accepted but extrapolation suspended",[]{Record a{10000,22600,0,9000,true,false};CHECK(acceptRecord(Kind::Boss,a,{10001,22601,0,10000,true,false}));CHECK(!a.verified);CHECK(calculate(Kind::Boss,a,10100).seconds==12501);CHECK(!calculate(Kind::Boss,a,22670).available);CHECK(acceptRecord(Kind::Boss,a,{22601,35201,0,22610,true,false}));CHECK(a.verified);});
 test("startup sync once not each focus change",[]{Coordinator c;c.activate(100,true);CHECK(c.takeDue(100,true)==7);for(int i=0;i<3;++i)c.complete(Kind(i),100,true);c.activate(110,false);CHECK(c.takeDue(110,true)==0);});
 test("event delayed twenty seconds and no five minute poll",[]{Coordinator c;c.records[2]={10000,11500,0,9000,true,false};c.activate(9990,true);CHECK(c.takeDue(9990,true)==7);for(int i=0;i<3;++i)c.complete(Kind(i),9990,true);c.reconcile(10000);CHECK(c.takeDue(10019,true)==0);CHECK(c.takeDue(10020,true)==4);c.complete(Kind::Legion,10020,true);c.reconcile(10320);CHECK(c.takeDue(10320,true)==0);});
 test("missed transitions coalesce while hidden",[]{Coordinator c;c.records[2]={10000,11500,0,9000,true,false};c.activate(9990,true);c.takeDue(9990,true);for(int i=0;i<3;++i)c.complete(Kind(i),9990,true);c.reconcile(12000);CHECK(c.takeDue(12000,false)==0);CHECK(c.takeDue(12000,true)==6);CHECK(c.takeDue(12000,true)==0);});
 test("failure backoff survives a new boundary",[]{Coordinator c;c.activate(3590,true);c.takeDue(3590,true);c.complete(Kind::Helltide,3590,false);c.reconcile(3600);CHECK(c.takeDue(3620,true)==0);CHECK(c.takeDue(3650,true)==2);c.complete(Kind::Helltide,3650,false);CHECK(c.takeDue(3769,true)==0);CHECK(c.takeDue(3770,true)==2);});
 test("Retry After and duplicate suppression",[]{Coordinator c;c.activate(100,true);CHECK(c.takeDue(100,true)==7);CHECK(c.takeDue(100,true)==0);c.complete(Kind::Boss,100,false,600);CHECK(c.takeDue(699,true)==0);CHECK(c.takeDue(700,true)==1);});
 test("settings clamp and preserve positive placement",[]{auto p=normalize({100,1,-10,-100,-100,false,true});CHECK(p.font==22&&p.width>=350&&p.opacity==10&&p.x==0&&p.y==0&&p.registered);});
 test("placement supports negative monitor origin",[]{auto b=place({-1920,0,1920,1080},270,82,1900,1060);CHECK(b.x==-270&&b.y==998&&b.width==270);});
 test("startup never recreates deleted entry",[]{CHECK(startupAction(false,false,false)==StartupAction::Create);CHECK(startupAction(true,false,false)==StartupAction::None);CHECK(startupAction(true,true,false)==StartupAction::Update);CHECK(startupAction(true,true,true)==StartupAction::None);});
 test("layout remains usable at large font and collapse",[]{auto p=normalize({22,270,85});auto l=layout(p,false);CHECK(l.width>=350&&l.height>=100);p.collapsed=true;CHECK(layout(p,false).width==28);CHECK(hitTest(p,false,10,10)==Hit::Collapse);});
 test("hit testing only rail and settings controls",[]{Preferences p;CHECK(hitTest(p,false,100,40)==Hit::None);CHECK(hitTest(p,false,10,10)==Hit::Collapse);CHECK(hitTest(p,false,10,38)==Hit::Drag);CHECK(hitTest(p,false,10,70)==Hit::Settings);});
 std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
