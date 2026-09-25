#include "render.h"
#include "platform.h"
#include <iostream>
#include <fstream>
#include <vector>
using namespace sanctuary;
#define REQUIRE(v) if(!(v)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#v<<'\n';return 1;}
int main(){
 auto foreground=GetForegroundWindow();
 auto hwnd=CreateWindowExW(WS_EX_LAYERED|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,L"STATIC",L"Sanctuary renderer test",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 REQUIRE(hwnd);
 Renderer r;REQUIRE(r.init());
 IDWriteFactory* factory{};REQUIRE(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(&factory))));
 for(int size=11;size<=22;++size){
  auto p=normalize({size,270,10});auto l=layout(p,false);IDWriteTextFormat* f{};
  REQUIRE(SUCCEEDED(factory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,static_cast<float>(size),L"en-us",&f)));
  f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  for(auto s:{L"ends ~55:00",L"~3:30:00",L"World Boss"}){IDWriteTextLayout* text{};REQUIRE(SUCCEEDED(factory->CreateTextLayout(s,static_cast<UINT32>(wcslen(s)),f,1000,1000,&text)));DWRITE_TEXT_METRICS m{};REQUIRE(SUCCEEDED(text->GetMetrics(&m)));REQUIRE(m.widthIncludingTrailingWhitespace<=(s[0]==L'W'?l.labelWidth:l.timeWidth));text->Release();}
  f->Release();std::array<Record,3> records{};
  REQUIRE(r.draw(hwnd,p,false,records,1790366400,1.f,{0,0,l.width,l.height},L"Offline estimates"));
 }
 factory->Release();
 r.discardDeviceResources();
 Preferences p{13,270,10};auto l=layout(p,false);
 REQUIRE(r.draw(hwnd,p,false,{},1790366400,1.f,{0,0,l.width,l.height},L"Recovered"));
 auto path=std::filesystem::temp_directory_path()/(L"SanctuaryTimers-render-"+std::to_wstring(GetCurrentProcessId())+L".bmp");
 REQUIRE(r.saveBitmap(path.wstring()));std::ifstream file(path,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});file.close();
 REQUIRE(bytes.size()>54+270*83*4-1);
 unsigned maxAlpha=0;for(size_t i=57;i<bytes.size();i+=4)maxAlpha=std::max(maxAlpha,unsigned(bytes[i]));
 auto backgroundAlpha=bytes[54+(15*270+140)*4+3];REQUIRE(maxAlpha>=250&&backgroundAlpha>=20&&backgroundAlpha<=32);
 std::filesystem::remove(path);
 REQUIRE(!IsWindowVisible(hwnd));REQUIRE(GetForegroundWindow()==foreground);DestroyWindow(hwnd);
 std::cout<<"PASS renderer: 12 font sizes, text metrics, transparent background with opaque text, resource recreation, hidden/no-activate\n";
}
