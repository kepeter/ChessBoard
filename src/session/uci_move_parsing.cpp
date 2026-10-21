/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "session/uci_move_parsing.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace chess {
namespace {
std::optional<PieceType> ParsePromotionLetter(char c) {
  switch (std::tolower(static_cast<unsigned char>(c))) {
    case 'q':
      return PieceType::kQueen;
    case 'r':
      return PieceType::kRook;
    case 'b':
      return PieceType::kBishop;
    case 'n':
      return PieceType::kKnight;
    default:
      return std::nullopt;
  }
}
}  // namespace

Square ResolveUciCastleSquare(const ChessEngine& engine, Square from,
                              Square to) {
  auto piece = engine.PieceAt(from);
  if (!piece || piece->type != PieceType::kKing || to.rank != from.rank)
    return to;

  std::optional<Square> translated;
  if (to.file ==
      *ChessEngine::CastleKingDestinationFile(MoveSpecial::kCastleKingSide))
    translated = Square(engine.RookStartFile(true), from.rank);
  else if (to.file == *ChessEngine::CastleKingDestinationFile(
                          MoveSpecial::kCastleQueenSide))
    translated = Square(engine.RookStartFile(false), from.rank);

  if (!translated) return to;

  auto legal = engine.GetLegalMoves(from);
  if (std::find(legal.begin(), legal.end(), to) != legal.end()) return to;
  if (std::find(legal.begin(), legal.end(), *translated) != legal.end())
    return *translated;

  // Neither reading is legal: return the original so the rejection reflects
  // what was reported.
  return to;
}

std::optional<Move> ApplyUciMove(ChessEngine& engine,
                                 const std::string& uci_move) {
  if (uci_move.size() < 4) return std::nullopt;

  Square from = Square::Parse(uci_move.substr(0, 2));
  Square to = ResolveUciCastleSquare(engine, from,
                                     Square::Parse(uci_move.substr(2, 2)));
  std::optional<PieceType> promotion =
      uci_move.size() >= 5 ? ParsePromotionLetter(uci_move[4]) : std::nullopt;

  return engine.TryMakeMove(from, to, promotion);
}
}  // namespace chess
