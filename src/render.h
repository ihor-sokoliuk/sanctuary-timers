#pragma once
#include "core.h"
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
namespace sanctuary {
class Renderer {
 ID2D1Factory* factory_{};IDWriteFactory* write_{};ID2D1DCRenderTarget* target_{};
 HDC dc_{};HBITMAP bitmap_{},old_{};void* pixels_{};int width_{},height_{};
 ID2D1SolidColorBrush* brush_{};
 bool createTarget();
 void color(unsigned rgb,float alpha=1);
 void text(const std::wstring&,float x,float y,float w,float h,float size,unsigned rgb,bool right=false,bool bold=false);
 void line(float x1,float y1,float x2,float y2,unsigned rgb,float width=1);
 void icon(int,float,float,unsigned);
 public:
 Renderer()=default;~Renderer();
 bool init();
 void discardDeviceResources();
 bool draw(HWND,Preferences,bool settings,const std::array<Record,3>&,Time now,float scale,Box bounds,const std::wstring& status);
 bool saveBitmap(const std::wstring&) const;
};
}
