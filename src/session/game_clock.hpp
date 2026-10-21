/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_SESSION_GAME_CLOCK_HPP_
#define CHESSBOARD_SESSION_GAME_CLOCK_HPP_

#include <chrono>
#include <optional>

#include "models/piece.hpp"
#include "models/time_control.hpp"

namespace chess {
// Tracks both players' remaining time. Time is deducted only in EndTurn() or
// Tick(), so the getters reflect the last update, not continuous real time.
class GameClock {
 public:
  std::chrono::milliseconds Increment() const {
    return time_control_ ? time_control_->Increment()
                         : std::chrono::milliseconds::zero();
  }

  // Set when a player's time reaches zero.
  std::optional<PieceColor> Flagged() const { return flagged_; }
  std::chrono::milliseconds WhiteRemaining() const { return white_remaining_; }
  std::chrono::milliseconds BlackRemaining() const { return black_remaining_; }

  void Start(TimeControl time_control);

  void EndTurn(PieceColor moved_color);

  // Recomputes side_to_move's time without ending the turn; call periodically
  // so the display counts down and a flag is detected even without a move.
  void Tick(PieceColor side_to_move);

 private:
  std::optional<TimeControl> time_control_;
  std::chrono::milliseconds white_remaining_{0};
  std::chrono::milliseconds black_remaining_{0};
  // steady_clock is immune to system clock changes.
  std::optional<std::chrono::steady_clock::time_point> turn_started_at_;
  std::optional<PieceColor> flagged_;

  std::chrono::milliseconds RemainingFor(PieceColor color) const;
  void SetRemaining(PieceColor color, std::chrono::milliseconds value);
};

}  // namespace chess

#endif  // CHESSBOARD_SESSION_GAME_CLOCK_HPP_
