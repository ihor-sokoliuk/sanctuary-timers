#include "platform.h"
namespace sanctuary {
Time utcNow(){return 0;}
Preferences loadPreferences(const std::filesystem::path&){return {};}
bool savePreferences(const std::filesystem::path&,Preferences){return false;}
std::array<Record,3> loadCache(const std::filesystem::path&,Time){return {};}
bool saveCache(const std::filesystem::path&,const std::array<Record,3>&){return false;}
Response fetchRecord(Kind,Time){return {};}
bool registerStartup(const std::filesystem::path&,Preferences&){return false;}
std::filesystem::path executablePath(){return {};}
bool isGameWindow(HWND){return false;}
Box gameBounds(HWND){return {};}
}
