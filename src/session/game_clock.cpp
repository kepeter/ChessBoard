/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "session/game_clock.hpp"

namespace chess {

void GameClock::Start(TimeControl time_control) {
  flagged_.reset();
  time_control_ = time_control;

  white_remaining_ = time_control_->InitialTime();
  black_remaining_ = time_control_->InitialTime();
  turn_started_at_ = std::chrono::steady_clock::now();
}

void GameClock::EndTurn(PieceColor moved_color) {
  if (!time_control_ || !turn_started_at_ || flagged_) return;

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - *turn_started_at_);
  auto remaining = RemainingFor(moved_color) - elapsed;

  // The increment doesn't rescue a player who ran out of time.
  if (remaining <= std::chrono::milliseconds::zero()) {
    SetRemaining(moved_color, std::chrono::milliseconds::zero());
    flagged_ = moved_color;
    return;
  }

  SetRemaining(moved_color, remaining + time_control_->Increment());

  turn_started_at_ = now;
}

void GameClock::Tick(PieceColor side_to_move) {
  if (!time_control_ || flagged_ || !turn_started_at_) return;

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - *turn_started_at_);
  // Advancing turn_started_at_ here keeps repeated ticks from double-counting
  // elapsed time.
  turn_started_at_ = now;

  auto remaining = RemainingFor(side_to_move) - elapsed;

  if (remaining <= std::chrono::milliseconds::zero()) {
    SetRemaining(side_to_move, std::chrono::milliseconds::zero());

    flagged_ = side_to_move;
  } else {
    SetRemaining(side_to_move, remaining);
  }
}

std::chrono::milliseconds GameClock::RemainingFor(PieceColor color) const {
  return color == PieceColor::kWhite ? white_remaining_ : black_remaining_;
}

void GameClock::SetRemaining(PieceColor color,
                             std::chrono::milliseconds value) {
  if (color == PieceColor::kWhite)
    white_remaining_ = value;
  else
    black_remaining_ = value;
}

}  // namespace chess
