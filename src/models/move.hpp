/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_MODELS_MOVE_HPP_
#define CHESSBOARD_MODELS_MOVE_HPP_

#include <optional>
#include <string>

#include "models/piece.hpp"
#include "models/square.hpp"

namespace chess {
// Moves needing more than "piece goes from A to B".
enum class MoveSpecial {
  kNone,
  kDoublePawnPush,
  kEnPassant,
  kCastleKingSide,
  kCastleQueenSide,
  kPromotion
};

struct Move {
  Square from;
  // For castling this is the rook's square, not the king's destination.
  Square to;
  Piece piece_moved{PieceType::kPawn, PieceColor::kWhite};
  std::optional<Piece> piece_captured;
  MoveSpecial special = MoveSpecial::kNone;
  std::optional<PieceType> promotion_type;

  // SAN ("Nf3", "exd5#"), filled in after the move is applied.
  std::string notation;

  bool is_check = false;
  bool is_checkmate = false;
};

}  // namespace chess

#endif  // CHESSBOARD_MODELS_MOVE_HPP_
