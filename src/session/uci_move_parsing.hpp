/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_SESSION_UCI_MOVE_PARSING_HPP_
#define CHESSBOARD_SESSION_UCI_MOVE_PARSING_HPP_

#include <optional>
#include <string>

#include "engine/chess_engine.hpp"
#include "models/square.hpp"

namespace chess {
// Applies one UCI move ("e2e4", "e7e8q") to `engine`, translating standard
// notation castling into the king-onto-rook form. Returns nullopt if the string
// is malformed or rejected.
std::optional<Move> ApplyUciMove(ChessEngine& engine,
                                 const std::string& uci_move);

// If `to` looks like a standard-notation castle for the king at `from`, returns
// the rook-square destination TryMakeMove expects; otherwise `to` unchanged.
// Engines only report king-onto-rook castling when UCI_Chess960 is set.
Square ResolveUciCastleSquare(const ChessEngine& engine, Square from,
                              Square to);
}  // namespace chess

#endif  // CHESSBOARD_SESSION_UCI_MOVE_PARSING_HPP_
