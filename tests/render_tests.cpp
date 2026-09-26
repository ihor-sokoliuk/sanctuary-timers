#include "render.h"
#include "platform.h"
#include <iostream>
#include <fstream>
#include <vector>
using namespace sanctuary;
#define REQUIRE(v) if(!(v)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#v<<'\n';return 1;}
int main(int argc,char** argv){
 auto foreground=GetForegroundWindow();
 auto hwnd=CreateWindowExW(WS_EX_LAYERED|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,L"STATIC",L"Sanctuary renderer test",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 REQUIRE(hwnd);
 Renderer r;REQUIRE(r.init());
 IDWriteFactory* factory{};REQUIRE(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(&factory))));
 // Exercise the renderer's real font collection, so system fallback cannot mask missing embedded fonts.
 for(bool bold:{false,true}){
  IDWriteTextFormat* format{};REQUIRE(SUCCEEDED(r.createTextFormat(15,bold,&format)));
  wchar_t familyName[64]{};REQUIRE(SUCCEEDED(format->GetFontFamilyName(familyName,64)));REQUIRE(std::wstring(familyName)==L"PT Serif");
  IDWriteFontCollection* collection{};REQUIRE(SUCCEEDED(format->GetFontCollection(&collection)));REQUIRE(collection);
  UINT32 index{};BOOL exists{};REQUIRE(SUCCEEDED(collection->FindFamilyName(L"PT Serif",&index,&exists)));REQUIRE(exists);
  IDWriteFontFamily* family{};REQUIRE(SUCCEEDED(collection->GetFontFamily(index,&family)));
  IDWriteFont* font{};REQUIRE(SUCCEEDED(family->GetFirstMatchingFont(bold?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STRETCH_NORMAL,DWRITE_FONT_STYLE_NORMAL,&font)));
  REQUIRE(font->GetSimulations()==DWRITE_FONT_SIMULATIONS_NONE);REQUIRE(font->GetWeight()==(bold?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL));
  font->Release();family->Release();collection->Release();format->Release();
 }
 for(int size=11;size<=22;++size){
  auto p=normalize({size,1,10});auto l=layout(p,false);IDWriteTextFormat* f{};
  REQUIRE(SUCCEEDED(r.createTextFormat(static_cast<float>(size),true,&f)));
  f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  for(auto s:{L"ends ~55:00",L"ends ~88:88",L"ends ~00:00",L"~23:59:59",L"~Now",L"in ~05:00",L"World Boss",L"Helltide",L"Legion"}){
   bool label=s[0]==L'W'||s[0]==L'H'||s[0]==L'L';
   IDWriteTextFormat* actualFormat{};REQUIRE(SUCCEEDED(r.createTextFormat(static_cast<float>(size),true,&actualFormat)));
   IDWriteTextLayout* text{};REQUIRE(SUCCEEDED(factory->CreateTextLayout(s,static_cast<UINT32>(wcslen(s)),actualFormat,1000,1000,&text)));
   DWRITE_TEXT_METRICS m{};REQUIRE(SUCCEEDED(text->GetMetrics(&m)));int available=label?l.labelWidth:l.timeWidth;
   if(m.widthIncludingTrailingWhitespace+1>available)std::wcerr<<L"Clipped at "<<size<<L" px: "<<s<<L" needs "<<m.widthIncludingTrailingWhitespace+1<<L", has "<<available<<'\n';
   REQUIRE(m.widthIncludingTrailingWhitespace+1<=available);text->Release();actualFormat->Release();
  }
  f->Release();std::array<Record,3> records{};
  REQUIRE(r.draw(hwnd,p,false,records,1790366400,1.f,{0,0,l.width,l.height},L"Offline estimates"));
 }
 for(auto message:{L"Clock: synced 1440 min ago (daily)",L"Clock: Windows; retry pending",L"Clock: cached; retry pending",L"Events checked: 99999 min ago"}){
  IDWriteTextFormat* format{};REQUIRE(SUCCEEDED(r.createTextFormat(11,true,&format)));
  IDWriteTextLayout* text{};REQUIRE(SUCCEEDED(factory->CreateTextLayout(message,static_cast<UINT32>(wcslen(message)),format,1000,1000,&text)));
  DWRITE_TEXT_METRICS metrics{};REQUIRE(SUCCEEDED(text->GetMetrics(&metrics)));REQUIRE(metrics.widthIncludingTrailingWhitespace+1<=layout({15,241,75},true).width-28);text->Release();format->Release();
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
 auto settingsLayout=layout({15,241,75},true);
 REQUIRE(r.draw(hwnd,{15,241,75},true,{},1790366400,1.5f,{0,0,settingsLayout.width*3/2,settingsLayout.height*3/2},L"Timers calculated locally",L"Clock: synced 1440 min ago (daily)"));
 if(argc==2)REQUIRE(r.saveBitmap(std::filesystem::path(argv[1]).wstring()));
 REQUIRE(!IsWindowVisible(hwnd));REQUIRE(GetForegroundWindow()==foreground);DestroyWindow(hwnd);
 std::cout<<"PASS renderer: embedded PT Serif regular/bold without fallback, compact minimum widths at 12 font sizes, text metrics, transparent background with opaque text, resource recreation, hidden/no-activate\n";
}
