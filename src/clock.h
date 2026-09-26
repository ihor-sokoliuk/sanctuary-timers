#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace sanctuary {
using Millis = std::int64_t;
using NtpPacket = std::array<std::uint8_t,48>;
struct ClockSample { Millis utcMillis{}; std::uint64_t ticks{}; Millis offsetMillis{},roundTripMillis{}; };
NtpPacket ntpRequest(Millis wallMillis);
std::optional<ClockSample> parseNtpReply(std::span<const std::uint8_t>,const NtpPacket&,Millis sent,Millis received,std::uint64_t sentTicks,std::uint64_t receivedTicks);
class DailyClock {
 std::optional<ClockSample> sample_;
 std::uint64_t next_{};
 bool busy_{};
 unsigned failures_{};
 public:
 static constexpr Millis Interval = 86400000;
 bool takeDue(std::uint64_t ticks);
 void finish(std::optional<ClockSample>,std::uint64_t ticks);
 Millis untilDue(std::uint64_t ticks) const;
 Millis now(Millis windowsMillis,std::uint64_t ticks) const;
 const std::optional<ClockSample>& sample() const {return sample_;}
 unsigned failures() const {return failures_;}
 std::wstring summary(std::uint64_t ticks) const;
};
}
