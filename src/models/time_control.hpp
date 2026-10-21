/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_MODELS_TIME_CONTROL_HPP_
#define CHESSBOARD_MODELS_TIME_CONTROL_HPP_

#include <array>
#include <chrono>
#include <string>

namespace chess {
enum class TimeControlCategory { kBullet, kBlitz, kRapid, kClassical };

struct TimeControl {
  int minutes;
  int increment_seconds;

  TimeControl(int minutes, int increment_seconds)
      : minutes(minutes), increment_seconds(increment_seconds) {}

  std::chrono::milliseconds InitialTime() const {
    return std::chrono::minutes(minutes);
  }
  std::chrono::milliseconds Increment() const {
    return std::chrono::seconds(increment_seconds);
  }

  std::string Label() const {
    return increment_seconds == 0 ? std::to_string(minutes) + "+0"
                                  : std::to_string(minutes) + "+" +
                                        std::to_string(increment_seconds);
  }

  // chess.com boundaries by starting minutes only: under 3 Bullet, up to 10
  // Blitz, under 60 Rapid, else Classical.
  TimeControlCategory Category() const {
    if (minutes < 3) return TimeControlCategory::kBullet;
    if (minutes <= 10) return TimeControlCategory::kBlitz;
    if (minutes < 60) return TimeControlCategory::kRapid;

    return TimeControlCategory::kClassical;
  }

  static const char* CategoryName(TimeControlCategory category) {
    switch (category) {
      case TimeControlCategory::kBullet:
        return "Bullet";
      case TimeControlCategory::kBlitz:
        return "Blitz";
      case TimeControlCategory::kRapid:
        return "Rapid";
      case TimeControlCategory::kClassical:
        return "Classical";
    }

    return "";
  }

  static const std::array<TimeControl, 12>& Presets() {
    static const std::array<TimeControl, 12> presets = {
        TimeControl(1, 0),   TimeControl(2, 1),   TimeControl(3, 0),
        TimeControl(3, 2),   TimeControl(5, 0),   TimeControl(5, 5),
        TimeControl(10, 0),  TimeControl(15, 10), TimeControl(30, 0),
        TimeControl(30, 20), TimeControl(60, 0),  TimeControl(90, 30),
    };

    return presets;
  }
};
}  // namespace chess

#endif  // CHESSBOARD_MODELS_TIME_CONTROL_HPP_
