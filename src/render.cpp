#include "render.h"
#include <fstream>
#include <filesystem>
#include <vector>
namespace sanctuary {
template<class T>static void release(T*& x){if(x){x->Release();x=nullptr;}}
Renderer::~Renderer(){if(dc_){SelectObject(dc_,old_);if(bitmap_)DeleteObject(bitmap_);DeleteDC(dc_);}release(brush_);release(target_);release(fonts_);if(write_&&fontLoader_)write_->UnregisterFontFileLoader(fontLoader_);release(fontLoader_);release(write_);release(factory_);}
bool Renderer::init(){
 if(FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&factory_)))return false;
 if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory5),reinterpret_cast<IUnknown**>(&write_)))||!loadFonts())return false;
 dc_=CreateCompatibleDC(nullptr);return dc_&&createTarget();
}
bool Renderer::loadFonts(){
 if(FAILED(write_->CreateInMemoryFontFileLoader(&fontLoader_))||FAILED(write_->RegisterFontFileLoader(fontLoader_)))return false;
 IDWriteFontSetBuilder1* builder{};if(FAILED(write_->CreateFontSetBuilder(&builder)))return false;
 bool ok=true;
 for(int id:{101,102}){
  auto module=GetModuleHandleW(nullptr);auto resource=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
  auto bytes=resource?LockResource(LoadResource(module,resource)):nullptr;
  auto length=resource?SizeofResource(module,resource):0;IDWriteFontFile* file{};
  if(!bytes||!length||FAILED(fontLoader_->CreateInMemoryFontFileReference(write_,bytes,length,nullptr,&file)))ok=false;
  else if(FAILED(builder->AddFontFile(file)))ok=false;
  release(file);if(!ok)break;
 }
 IDWriteFontSet* set{};
 if(ok)ok=SUCCEEDED(builder->CreateFontSet(&set));
 if(ok)ok=SUCCEEDED(write_->CreateFontCollectionFromFontSet(set,&fonts_));
 release(set);release(builder);return ok;
}
void Renderer::discardDeviceResources(){release(brush_);release(target_);}
bool Renderer::createTarget(){
 auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
 if(FAILED(factory_->CreateDCRenderTarget(&properties,&target_)))return false;
 if(FAILED(target_->CreateSolidColorBrush(D2D1::ColorF(1.f,1.f,1.f),&brush_))){discardDeviceResources();return false;}
 return true;
}
void Renderer::color(unsigned rgb,float alpha){brush_->SetColor(D2D1::ColorF(rgb,alpha));}
HRESULT Renderer::createTextFormat(float size,bool bold,IDWriteTextFormat** format){
 return write_->CreateTextFormat(L"PT Serif",fonts_,bold?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",format);
}
void Renderer::text(const std::wstring& s,float x,float y,float w,float h,float size,unsigned rgb,bool right,bool bold){
 IDWriteTextFormat* font{};
 if(FAILED(createTextFormat(size,bold,&font)))return;
 font->SetTextAlignment(right?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_LEADING);font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);font->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
 color(0x000000,.8f);target_->DrawText(s.c_str(),static_cast<UINT32>(s.size()),font,D2D1::RectF(x+.8f,y+.8f,x+w+.8f,y+h+.8f),brush_,D2D1_DRAW_TEXT_OPTIONS_CLIP);
 color(rgb);target_->DrawText(s.c_str(),static_cast<UINT32>(s.size()),font,D2D1::RectF(x,y,x+w,y+h),brush_,D2D1_DRAW_TEXT_OPTIONS_CLIP);font->Release();
}
void Renderer::line(float x1,float y1,float x2,float y2,unsigned rgb,float width){color(rgb);target_->DrawLine(D2D1::Point2F(x1,y1),D2D1::Point2F(x2,y2),brush_,width);}
void Renderer::icon(int i,float x,float y,unsigned rgb){
 color(rgb);
 if(i==0){target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x,y-1),6,5.8f),brush_);target_->FillRectangle(D2D1::RectF(x-3.5f,y+1,x+3.5f,y+7),brush_);color(0x14171e);target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x-2.5f,y-1),1.6f,1.7f),brush_);target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x+2.5f,y-1),1.6f,1.7f),brush_);line(x,y+4,x,y+7,0x14171e);}
 else if(i==1){ID2D1PathGeometry* path{};if(SUCCEEDED(factory_->CreatePathGeometry(&path))){ID2D1GeometrySink* sink{};if(SUCCEEDED(path->Open(&sink))){sink->BeginFigure(D2D1::Point2F(x,y-8),D2D1_FIGURE_BEGIN_FILLED);sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x+1,y-1),D2D1::Point2F(x+9,y),D2D1::Point2F(x+5,y+6)));sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x+1,y+10),D2D1::Point2F(x-8,y+6),D2D1::Point2F(x-5,y)));sink->AddLine(D2D1::Point2F(x-3,y-4));sink->AddLine(D2D1::Point2F(x-2,y));sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();target_->FillGeometry(path,brush_);sink->Release();}path->Release();}}
 else {line(x-6,y-7,x+5,y+6,rgb,2);line(x+6,y-7,x-5,y+6,rgb,2);line(x-7,y+3,x-2,y+7,rgb,1.5f);line(x+7,y+3,x+2,y+7,rgb,1.5f);}
}
bool Renderer::draw(HWND hwnd,Preferences p,bool settings,const std::array<Record,3>& records,Time now,float scale,Box bounds,const std::wstring& status,const std::wstring& clockStatus){
 if(bounds.width<1||bounds.height<1)return false;if(!target_&&!createTarget())return false;
 if(width_!=bounds.width||height_!=bounds.height){
  if(bitmap_){SelectObject(dc_,old_);DeleteObject(bitmap_);bitmap_=nullptr;}
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=bounds.width;info.bmiHeader.biHeight=-bounds.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&pixels_,nullptr,0);if(!bitmap_)return false;
  old_=static_cast<HBITMAP>(SelectObject(dc_,bitmap_));width_=bounds.width;height_=bounds.height;
 }
 RECT area{0,0,width_,height_};if(FAILED(target_->BindDC(dc_,&area)))return false;
 target_->BeginDraw();target_->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale));target_->Clear(D2D1::ColorF(0,0.f));target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
 auto l=layout(p,settings);float w=static_cast<float>(l.width),h=static_cast<float>(l.height);
 color(0x10141c,p.opacity/100.f);target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0,0,w,h),7,7),brush_);
 color(0x536074,0.5f);target_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(.5f,.5f,w-.5f,h-.5f),7,7),brush_,1);
 if(settings){
  text(L"‹   Appearance",12,4,w-24,28,13,0xf1e7de,false,true);
  const wchar_t* labels[]={L"Text size",L"Panel width",L"Background"};int values[]={p.font,p.width,p.opacity};
  for(int i=0;i<3;++i){float y=42.f+i*40;text(labels[i],14,y,130,32,12,0xc2c8d0);text(std::to_wstring(values[i])+(i==2?L"%":L" px"),140,y,w-232,32,12,0xf1e7de,true);
   color(0x536074,.3f);target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(w-82,y,w-48,y+32),4,4),brush_);target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(w-42,y,w-8,y+32),4,4),brush_);
   text(L"−",w-71,y,20,32,16,0xf1e7de);text(L"+",w-31,y,20,32,16,0xf1e7de);
  }
  text(status,14,169,w-28,25,11,0xb0bac9);
  Time checked=0;for(const auto& r:records)if(r.checked&&(checked==0||r.checked<checked))checked=r.checked;
  auto last=checked?L"Events checked: "+std::to_wstring(std::max<Time>(0,now-checked)/60)+L" min ago":L"Events: waiting for first sync";
  text(last,14,194,w-28,21,11,0xb0bac9);text(clockStatus,14,215,w-28,21,11,0xb0bac9);
  text(L"Startup: managed in Task Manager",14,236,w-28,21,10,0x8995a7);
 }else{
  if(!p.collapsed)line(28,8,28,h-8,0x394252);
  text(p.collapsed?L"›":L"‹",9,0,18,27,20,0xe8c3ae);
  for(int yy=-1;yy<=1;++yy){color(0x91a0b5);target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(11,h/2+yy*5),1,1),brush_);target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(16,h/2+yy*5),1,1),brush_);}
  color(0xa7b2c4);target_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(14,h-14),5,5),brush_,1.3f);target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(14,h-14),1.7f,1.7f),brush_);
  for(int i=0;i<4;++i){float dx=i<2?(i?7.f:-7.f):0,dy=i>=2?(i==2?-7.f:7.f):0;line(14+dx*.7f,h-14+dy*.7f,14+dx,h-14+dy,0xa7b2c4,1.3f);}
  if(!p.collapsed){const wchar_t* names[]={L"World Boss",L"Helltide",L"Legion"};unsigned colors[]={0xdfa89d,0xf2b566,0xa7bfca};
   for(int i=0;i<3;++i){float y=4.f+i*l.row;auto v=calculate(Kind(i),records[i],now);unsigned rowColor=colors[i];icon(i,42,y+l.row/2.f,colors[i]);text(names[i],58,y,static_cast<float>(l.labelWidth),static_cast<float>(l.row),static_cast<float>(p.font),rowColor);
    auto value=formatTime(v);if(i==1)value=(v.active?L"ends ":L"in ")+value;
    text(value,w-l.timeWidth-8,y,static_cast<float>(l.timeWidth),static_cast<float>(l.row),static_cast<float>(p.font),rowColor,true,true);
   }
  }
 }
 auto end=target_->EndDraw();if(FAILED(end)){if(end==D2DERR_RECREATE_TARGET)discardDeviceResources();return false;}
 POINT pos{bounds.x,bounds.y},source{0,0};SIZE size{width_,height_};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
 return UpdateLayeredWindow(hwnd,nullptr,&pos,&size,dc_,&source,0,&blend,ULW_ALPHA)!=0;
}
bool Renderer::saveBitmap(const std::wstring& file)const{
 if(!pixels_)return false;
 BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=width_;info.biHeight=-height_;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
 header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+width_*height_*4;
 std::ofstream f(std::filesystem::path(file),std::ios::binary);f.write(reinterpret_cast<const char*>(&header),sizeof(header));f.write(reinterpret_cast<const char*>(&info),sizeof(info));f.write(static_cast<const char*>(pixels_),width_*height_*4);return bool(f);
}
}
