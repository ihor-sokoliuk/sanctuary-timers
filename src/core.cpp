#include "core.h"
#include <algorithm>
#include <chrono>
#include <cwchar>
#include <map>
#include <stdexcept>

namespace sanctuary {
namespace {
Time floorDiv(Time a,Time b) { auto q=a/b; return q-((a%b)<0); }
struct Value { bool isString{}; std::string text; };
// Only top-level fields are retained. All other JSON is still structurally validated.
class Json {
 std::string_view s; size_t p{};
 [[noreturn]] void bad() { throw std::runtime_error("Invalid event JSON"); }
 void ws() { while(p<s.size()&&(s[p]==' '||s[p]=='\r'||s[p]=='\n'||s[p]=='\t')) ++p; }
 bool eat(char c) { ws(); if(p<s.size()&&s[p]==c){++p;return true;}return false; }
 void need(char c) { if(!eat(c))bad(); }
 std::string str() {
  need('"');std::string out;
  while(p<s.size()) {
   unsigned char c=static_cast<unsigned char>(s[p++]);if(c=='"')return out;if(c<32)bad();
   if(c=='\\') {
    if(p==s.size())bad();char e=s[p++];
    if(e=='u') {
     unsigned code=0;for(int i=0;i<4;++i){if(p==s.size())bad();char h=s[p++];int n=h>='0'&&h<='9'?h-'0':h>='a'&&h<='f'?h-'a'+10:h>='A'&&h<='F'?h-'A'+10:-1;if(n<0)bad();code=code*16+n;}
     out+=code<128?static_cast<char>(code):'?';
    } else if(e=='"'||e=='\\'||e=='/')out+=e;
    else if(e=='n')out+='\n';else if(e=='r')out+='\r';else if(e=='t')out+='\t';else if(e=='b')out+='\b';else if(e=='f')out+='\f';else bad();
   } else out+=static_cast<char>(c);
  }bad();
 }
 Value value(int depth) {
  if(depth>16)bad();ws();if(p==s.size())bad();
  if(s[p]=='"')return {true,str()};
  if(eat('{')) {if(!eat('}')){do{str();need(':');value(depth+1);}while(eat(','));need('}');}return {};}
  if(eat('[')) {if(!eat(']')){do{value(depth+1);}while(eat(','));need(']');}return {};}
  for(auto literal:{"null","true","false"}) {std::string_view l=literal;if(s.substr(p,l.size())==l){p+=l.size();return {};}}
  if(s[p]=='-')++p;if(p==s.size())bad();
  if(s[p]=='0')++p;else {if(s[p]<'1'||s[p]>'9')bad();while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;}
  if(p<s.size()&&s[p]=='.'){++p;size_t begin=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==begin)bad();}
  if(p<s.size()&&(s[p]=='e'||s[p]=='E')){++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;size_t begin=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==begin)bad();}
  return {};
 }
 public:
 explicit Json(std::string_view input):s(input){}
 std::map<std::string,Value> root() {
  if(s.size()>65536)bad();std::map<std::string,Value> out;need('{');
  if(!eat('}')){do{auto k=str();need(':');if(!out.emplace(k,value(1)).second)bad();}while(eat(','));need('}');}
  ws();if(p!=s.size())bad();return out;
 }
};
}
Time period(Kind k) {return k==Kind::Boss?12600:k==Kind::Legion?1500:3600;}
bool RequestBudget::permits(std::uint64_t ticks,bool allowed)const{return allowed&&ticks-started<20000;}
std::optional<Time> parseUtc(std::string_view s) {
 if(s.size()!=20&&s.size()!=24)return {};
 if(s[4]!='-'||s[7]!='-'||s[10]!='T'||s[13]!=':'||s[16]!=':'||s.back()!='Z')return {};
 auto number=[&](size_t p,size_t n)->int{int v=0;for(size_t i=0;i<n;++i){char c=s[p+i];if(c<'0'||c>'9')return -1;v=v*10+c-'0';}return v;};
 if(s.size()==24&&(s[19]!='.'||number(20,3)<0))return {};
 int y=number(0,4),m=number(5,2),d=number(8,2),h=number(11,2),mi=number(14,2),se=number(17,2);
 if(y<1970||y>2099||m<1||m>12||d<1||d>31||h<0||h>23||mi<0||mi>59||se<0||se>59)return {};
 std::chrono::year_month_day date{std::chrono::year(y),std::chrono::month(static_cast<unsigned>(m)),std::chrono::day(static_cast<unsigned>(d))};
 if(!date.ok())return {};
 return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::sys_days(date).time_since_epoch()).count()+h*3600+mi*60+se;
}
std::optional<Record> parseRecord(Kind k,std::string_view json,Time now) {
 try {
  auto fields=Json(json).root();
  auto time=[&](const char* key){auto i=fields.find(key);return i!=fields.end()&&i->second.isString?parseUtc(i->second.text):std::optional<Time>{};};
  auto start=time("startTime"),next=time("nextTime"),end=time("endTime");
  if(!start||*start<now-period(k)*2||*start>now+period(k)*2)return {};
  Record r{*start,next.value_or(0),end.value_or(0),now,false,false};
  if(k==Kind::Helltide) {
   if(!end||*end-*start!=3300||*start%3600!=0)return {};
   r.verified=true;
  }else {
   if(!next||*next<=*start||*next-*start>86400)return {};
   r.verified=*next-*start==period(k);
  }
  return r;
 }catch(...){return {};}
}
bool acceptRecord(Kind k,Record& a,const Record& b) {
 if(a.start&&b.start<a.start)return false;
 if(k!=Kind::Helltide&&!b.verified&&b.next<=b.checked)return false;
 bool phaseChanged=a.start&&k!=Kind::Helltide&&(b.start-a.start)%period(k)!=0;
 a=b;if(phaseChanged)a.verified=false;return true;
}
View calculate(Kind k,const Record& r,Time now) {
 View v;v.estimated=r.failed||!r.checked;
 if(k==Kind::Helltide) {
  auto phase=now-floorDiv(now,3600)*3600;v.available=true;v.active=phase<3300;
  v.seconds=(v.active?3300:3600)-phase;v.boundary=now+v.seconds;return v;
 }
 if(!r.start)return v;
 Time previous=0,next=0;
 if(r.verified) {previous=r.start+floorDiv(now-r.start,period(k))*period(k);next=previous+period(k);}
 else {
  v.estimated=true;
  if(now<r.start)next=r.start;
  else if(r.next&&now<r.next){previous=r.start;next=r.next;}
  else if(r.next&&now<r.next+60)previous=r.next;
  else return v;
 }
 v.now=previous&&now>=previous&&now-previous<60;v.available=v.now||next>now;
 v.seconds=v.now?0:next-now;v.boundary=next?next:Never;return v;
}
std::wstring formatTime(const View& v) {
 if(!v.available)return L"—";
 if(v.now)return v.estimated?L"~Now":L"Now";
 wchar_t b[40];auto s=std::max<Time>(0,v.seconds);
 if(s>=3600)std::swprintf(b,40,L"%lld:%02lld:%02lld",static_cast<long long>(s/3600),static_cast<long long>(s/60%60),static_cast<long long>(s%60));
 else std::swprintf(b,40,L"%02lld:%02lld",static_cast<long long>(s/60),static_cast<long long>(s%60));
 return (v.estimated?L"~":L"")+std::wstring(b);
}
void Coordinator::activate(Time now,bool newSession) {
 if(newSession)for(int i=0;i<3;++i){if(!requests[i].busy&&!requests[i].failures)requests[i].due=now;boundaries_[i]=calculate(Kind(i),records[i],now).boundary;}
 reconcile(now);
}
void Coordinator::reconcile(Time now) {
 for(int i=0;i<3;++i) {
  auto& q=requests[i];
  if(now>=boundaries_[i]&&!q.failures&&!q.busy)q.due=std::min(q.due,boundaries_[i]+20);
  boundaries_[i]=calculate(Kind(i),records[i],now).boundary;
 }
}
bool Coordinator::observeClock(Time now,std::uint64_t ticks){
 Time drift=observed_?(now-wall_)-static_cast<Time>((ticks-ticks_)/1000):0;
 bool jumped=observed_&&(drift>5||drift<-5);
 if(jumped)for(auto& q:requests)if(q.failures&&q.due!=Never)q.due=std::max(now,q.due+drift);
 wall_=now;ticks_=ticks;observed_=true;if(jumped)activate(now,true);return jumped;
}
int Coordinator::takeDue(Time now,bool foreground) {
 if(!foreground)return 0;int mask=0;
 for(int i=0;i<3;++i){auto& q=requests[i];if(!q.busy&&q.due<=now){q.busy=true;mask|=1<<i;}}
 return mask;
}
void Coordinator::complete(Kind k,Time now,bool success,Time retryAfter) {
 auto i=static_cast<int>(k);auto& q=requests[i];q.busy=false;
 if(success){q.failures=0;q.due=Never;records[i].failed=false;}
 else {q.failures=std::min(10u,q.failures+1);records[i].failed=true;auto delay=std::min<Time>(1800,60LL<<(q.failures-1));q.due=now+std::max(delay,std::clamp<Time>(retryAfter,0,7*86400));}
 boundaries_[i]=calculate(k,records[i],now).boundary;
}
Time Coordinator::nextWake(Time now) const {
 Time due=Never;for(int i=0;i<3;++i){if(!requests[i].busy)due=std::min(due,requests[i].due);due=std::min(due,boundaries_[i]);}return due==Never?Never:std::max(now+1,due);
}
// Conservative bold PT Serif advances: 5.33 em for labels, 5.64 em for timers.
// Round each column separately and leave 1 px for its shadow, plus the 8 px gap.
static int timerColumnWidth(int font){return (font*564+99)/100+1;}
Preferences normalize(Preferences p) {p.font=std::clamp(p.font,11,22);p.width=std::clamp(p.width,75+(p.font*533+99)/100+timerColumnWidth(p.font),560);p.opacity=std::clamp(p.opacity,10,100);p.x=std::clamp(p.x,0,16000);p.y=std::clamp(p.y,0,16000);return p;}
Box place(Box game,int width,int height,int x,int y) {width=std::max(1,std::min(width,game.width));height=std::max(1,std::min(height,game.height));return {game.x+std::clamp(x,0,game.width-width),game.y+std::clamp(y,0,game.height-height),width,height};}
StartupAction startupAction(bool registered,bool exists,bool samePath) {if(!exists)return registered?StartupAction::None:StartupAction::Create;return samePath?StartupAction::None:StartupAction::Update;}
Layout layout(Preferences p,bool settings) {p=normalize(p);int row=std::max(24,p.font+12);int width=settings?std::max(320,p.width):p.collapsed?28:p.width;int timeWidth=timerColumnWidth(p.font);return {width,settings?246:row*3+8,28,row,timeWidth,width-58-timeWidth-16};}
Hit hitTest(Preferences p,bool settings,int x,int y) {
 auto l=layout(p,settings);if(x<0||y<0||x>=l.width||y>=l.height)return Hit::None;
 if(settings){if(y<32)return Hit::Back;for(int i=0;i<3;++i)if(y>=42+i*40&&y<74+i*40){if(x>=l.width-82&&x<l.width-48)return Hit(int(Hit::FontDown)+i*2);if(x>=l.width-42)return Hit(int(Hit::FontUp)+i*2);}return Hit::None;}
 if(x>=28)return Hit::None;if(y<28)return Hit::Collapse;if(y>=l.height-28)return Hit::Settings;return Hit::Drag;
}
}
