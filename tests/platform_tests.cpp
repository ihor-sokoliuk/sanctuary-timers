#include "platform.h"
#include <fstream>
#include <iostream>
using namespace sanctuary;
#define REQUIRE(v) if(!(v)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#v<<'\n';return 1;}
int main() {
 auto folder=std::filesystem::temp_directory_path()/(L"SanctuaryTimers-tests-"+std::to_wstring(GetCurrentProcessId()));
 std::filesystem::create_directories(folder);
 auto p=folder/L"settings.ini";
 REQUIRE(loadPreferences(p).font==13);
 Preferences original{18,410,43,72,88,true,true};
 REQUIRE(savePreferences(p,original));
 auto copy=loadPreferences(p);
 REQUIRE(copy.font==18&&copy.width==410&&copy.opacity==43&&copy.x==72&&copy.y==88&&copy.collapsed&&copy.registered);
 std::ofstream(p)<<"[appearance]\nfont=999\nwidth=-1\nopacity=999\nx=-1\n";
 copy=loadPreferences(p);REQUIRE(copy.font==22&&copy.width>=350&&copy.opacity==100&&copy.x==0);
 std::array<Record,3> records{};records[0]={1790370000,1790382600,0,1790369000,true,false};
 p=folder/L"events.ini";REQUIRE(saveCache(p,records));
 auto cache=loadCache(p,1790370100);
 REQUIRE(cache[0].start==1790370000&&cache[0].verified&&cache[0].failed);
 REQUIRE(!calculate(Kind::Legion,cache[2],1790370100).available);
 std::ofstream(p)<<"[0]\nstart=999999999999999999\nnext=-1\nchecked=1\nverified=1\n";
 cache=loadCache(p,1790370100); REQUIRE(cache[0].start==0);
 REQUIRE(std::filesystem::is_regular_file(executablePath()));
 REQUIRE(!isGameWindow(nullptr));
 std::filesystem::remove_all(folder);
 std::cout<<"PASS preferences roundtrip, corrupt preferences, cache roundtrip, corrupt cache, executable path, null window\n";
 return 0;
}
