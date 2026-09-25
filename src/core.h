#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sanctuary {
using Time = std::int64_t;
enum class Kind { Boss, Helltide, Legion };
constexpr int bit(Kind k) { return 1 << static_cast<int>(k); }
constexpr Time Never = INT64_MAX / 4;
struct Record { Time start{}, next{}, end{}, checked{}; bool verified{}, failed{}; };
struct View { Time seconds{}, boundary{Never}; bool active{}, now{}, estimated{}, available{}; };
Time period(Kind);
std::optional<Time> parseUtc(std::string_view);
std::optional<Record> parseRecord(Kind, std::string_view, Time now);
bool acceptRecord(Kind, Record& current, const Record& incoming);
View calculate(Kind, const Record&, Time now);
std::wstring formatTime(const View&);

struct Pending { Time due{Never}; unsigned failures{}; bool busy{}; };
class Coordinator {
 public:
  std::array<Record,3> records{};
  std::array<Pending,3> requests{};
  void activate(Time now, bool newSession);
  void reconcile(Time now);
  int takeDue(Time now, bool foreground);
  void complete(Kind, Time now, bool success, Time retryAfter=0);
  Time nextWake(Time now) const;
 private:
  std::array<Time,3> boundaries_{Never,Never,Never};
};
struct Preferences { int font{13}, width{270}, opacity{85}, x{24}, y{24}; bool collapsed{}, registered{}; };
Preferences normalize(Preferences);
struct Box { int x{},y{},width{},height{}; };
Box place(Box game, int width, int height, int x, int y);
enum class StartupAction { None, Create, Update };
StartupAction startupAction(bool registered, bool exists, bool samePath);
enum class Hit { None, Collapse, Drag, Settings, Back, FontDown, FontUp, WidthDown, WidthUp, OpacityDown, OpacityUp };
struct Layout { int width{},height{},rail{},row{}; };
Layout layout(Preferences, bool settings);
Hit hitTest(Preferences, bool settings, int x, int y);
}
