#include "clock.h"
#include <algorithm>
#include <cstdlib>
namespace sanctuary {
namespace {
constexpr Millis Epoch=2208988800LL,Era=4294967296LL;
std::uint32_t word(std::span<const std::uint8_t> p,size_t at){std::uint32_t n=0;for(size_t i=0;i<4;++i)n=(n<<8)|p[at+i];return n;}
Millis timestamp(std::span<const std::uint8_t> p,size_t at,Millis reference){
 Millis seconds=word(p,at)-Epoch;
 while(seconds-reference/1000>Era/2)seconds-=Era;
 while(reference/1000-seconds>Era/2)seconds+=Era;
 return seconds*1000+static_cast<Millis>((static_cast<std::uint64_t>(word(p,at+4))*1000+(1ULL<<31))>>32);
}
bool zero(std::span<const std::uint8_t> p,size_t at){return word(p,at)==0&&word(p,at+4)==0;}
}
NtpPacket ntpRequest(Millis wall){
 NtpPacket p{};p[0]=0x23;
 auto seconds=static_cast<std::uint32_t>(wall/1000+Epoch);
 auto fraction=static_cast<std::uint32_t>(((static_cast<std::uint64_t>(wall%1000)<<32)+500)/1000);
 for(size_t i=0;i<4;++i){p[40+i]=static_cast<std::uint8_t>(seconds>>(24-8*i));p[44+i]=static_cast<std::uint8_t>(fraction>>(24-8*i));}
 return p;
}
std::optional<ClockSample> parseNtpReply(std::span<const std::uint8_t> p,const NtpPacket& q,Millis sent,Millis received,std::uint64_t sentTicks,std::uint64_t receivedTicks){
 if(p.size()<48||p.size()>512||sent<1577836800000LL||sent>4102444800000LL||receivedTicks<sentTicks||receivedTicks-sentTicks>2000)return {};
 auto version=(p[0]>>3)&7;
 if((p[0]>>6)==3||(version!=3&&version!=4)||(p[0]&7)!=4||p[1]==0||p[1]>15)return {};
 if(!std::equal(q.begin()+40,q.end(),p.begin()+24)||zero(p,32)||zero(p,40))return {};
 auto elapsed=static_cast<Millis>(receivedTicks-sentTicks);
 if(std::abs((received-sent)-elapsed)>100)return {};
 auto receive=timestamp(p,32,sent),transmit=timestamp(p,40,sent);
 if(transmit<receive)return {};
 auto delay=(received-sent)-(transmit-receive);
 auto offset=((receive-sent)+(transmit-received))/2;
 if(delay<0||delay>1500||std::abs(offset)>300000)return {};
 return ClockSample{received+offset,receivedTicks,offset,delay};
}
bool DailyClock::takeDue(std::uint64_t ticks){if(busy_||ticks<next_)return false;busy_=true;return true;}
void DailyClock::finish(std::optional<ClockSample> sample,std::uint64_t ticks){
 busy_=false;
 if(sample){sample_=sample;failures_=0;next_=ticks+Interval;}
 else {failures_=std::min(failures_+1,10u);next_=ticks+std::min<Millis>(21600000,900000LL<<std::min(failures_-1,5u));}
}
Millis DailyClock::untilDue(std::uint64_t ticks)const{return busy_?Interval:next_>ticks?static_cast<Millis>(next_-ticks):0;}
Millis DailyClock::now(Millis windows,std::uint64_t ticks)const{return sample_&&ticks>=sample_->ticks?sample_->utcMillis+static_cast<Millis>(ticks-sample_->ticks):windows;}
std::wstring DailyClock::summary(std::uint64_t ticks)const{
 if(busy_)return L"Clock: syncing...";
 if(failures_)return sample_?L"Clock: cached; retry pending":L"Clock: Windows; retry pending";
 if(!sample_)return L"Clock: waiting for sync";
 auto age=ticks>=sample_->ticks?(ticks-sample_->ticks)/60000:0;
 return L"Clock: synced "+std::to_wstring(age)+L" min ago (daily)";
}
}
